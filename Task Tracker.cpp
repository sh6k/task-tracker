#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>


// ─── Constants ───────────────────────────────────────────────────────────────
const std::string TASKS_FILE = "tasks.json";

// ─── Task struct ─────────────────────────────────────────────────────────────
struct Task {
    int id;
    std::string description;
    std::string status;      // "todo" | "in-progress" | "done"
    std::string createdAt;
    std::string updatedAt;
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string currentTimestamp() {
    std::time_t now = std::time(nullptr);
    struct tm timeInfo;
    localtime_s(&timeInfo, &now);   // MSVC-safe version
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return std::string(buf);
}
// Escape quotes inside a string for JSON
std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

// Unescape JSON string value
std::string unescapeJson(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == '"') { out += '"';  ++i; }
            else if (s[i + 1] == '\\') { out += '\\'; ++i; }
            else out += s[i];
        }
        else {
            out += s[i];
        }
    }
    return out;
}

// Extract value of a JSON key from a single-object string
// e.g. extractField(obj, "description") => "Buy groceries"
std::string extractField(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = obj.find(search);
    if (pos == std::string::npos) return "";

    pos = obj.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    ++pos;

    // Skip whitespace
    while (pos < obj.size() && obj[pos] == ' ') ++pos;

    if (obj[pos] == '"') {
        // String value
        ++pos;
        std::string val;
        while (pos < obj.size()) {
            if (obj[pos] == '\\' && pos + 1 < obj.size()) {
                val += '\\'; val += obj[pos + 1];
                pos += 2;
            }
            else if (obj[pos] == '"') {
                break;
            }
            else {
                val += obj[pos++];
            }
        }
        return unescapeJson(val);
    }
    else {
        // Number or bare value
        size_t end = obj.find_first_of(",}", pos);
        std::string val = obj.substr(pos, end - pos);
        // trim
        val.erase(val.find_last_not_of(" \t\n") + 1);
        return val;
    }
}

// ─── JSON I/O ─────────────────────────────────────────────────────────────────

std::string taskToJson(const Task& t, const std::string& indent = "  ") {
    return indent + "{\n" +
        indent + "  \"id\": " + std::to_string(t.id) + ",\n" +
        indent + "  \"description\": \"" + escapeJson(t.description) + "\",\n" +
        indent + "  \"status\": \"" + t.status + "\",\n" +
        indent + "  \"createdAt\": \"" + t.createdAt + "\",\n" +
        indent + "  \"updatedAt\": \"" + t.updatedAt + "\"\n" +
        indent + "}";
}

std::vector<Task> loadTasks() {
    std::vector<Task> tasks;

    std::ifstream file(TASKS_FILE);
    if (!file.is_open()) return tasks;   // File doesn't exist yet — empty list

    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    // Parse array of objects: find each { ... } block
    size_t pos = 0;
    while (pos < content.size()) {
        size_t start = content.find('{', pos);
        if (start == std::string::npos) break;

        // Find matching closing brace (handles no nesting needed here)
        size_t end = content.find('}', start);
        if (end == std::string::npos) break;

        std::string obj = content.substr(start, end - start + 1);

        Task t;
        std::string idStr = extractField(obj, "id");
        if (idStr.empty()) { pos = end + 1; continue; }

        t.id = std::stoi(idStr);
        t.description = extractField(obj, "description");
        t.status = extractField(obj, "status");
        t.createdAt = extractField(obj, "createdAt");
        t.updatedAt = extractField(obj, "updatedAt");

        tasks.push_back(t);
        pos = end + 1;
    }

    return tasks;
}

void saveTasks(const std::vector<Task>& tasks) {
    std::ofstream file(TASKS_FILE);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write to " << TASKS_FILE << "\n";
        return;
    }

    file << "[\n";
    for (size_t i = 0; i < tasks.size(); ++i) {
        file << taskToJson(tasks[i]);
        if (i + 1 < tasks.size()) file << ",";
        file << "\n";
    }
    file << "]\n";
}

int nextId(const std::vector<Task>& tasks) {
    int maxId = 0;
    for (const auto& t : tasks)
        if (t.id > maxId) maxId = t.id;
    return maxId + 1;
}

// ─── Commands ─────────────────────────────────────────────────────────────────

void cmdAdd(const std::string& description) {
    auto tasks = loadTasks();
    Task t;
    t.id = nextId(tasks);
    t.description = description;
    t.status = "todo";
    t.createdAt = currentTimestamp();
    t.updatedAt = t.createdAt;
    tasks.push_back(t);
    saveTasks(tasks);
    std::cout << "Task added successfully (ID: " << t.id << ")\n";
}

void cmdUpdate(int id, const std::string& newDesc) {
    auto tasks = loadTasks();
    for (auto& t : tasks) {
        if (t.id == id) {
            t.description = newDesc;
            t.updatedAt = currentTimestamp();
            saveTasks(tasks);
            std::cout << "Task " << id << " updated successfully.\n";
            return;
        }
    }
    std::cerr << "Error: Task with ID " << id << " not found.\n";
}

void cmdDelete(int id) {
    auto tasks = loadTasks();
    auto it = std::remove_if(tasks.begin(), tasks.end(),
        [id](const Task& t) { return t.id == id; });
    if (it == tasks.end()) {
        std::cerr << "Error: Task with ID " << id << " not found.\n";
        return;
    }
    tasks.erase(it, tasks.end());
    saveTasks(tasks);
    std::cout << "Task " << id << " deleted successfully.\n";
}

void cmdMark(int id, const std::string& status) {
    auto tasks = loadTasks();
    for (auto& t : tasks) {
        if (t.id == id) {
            t.status = status;
            t.updatedAt = currentTimestamp();
            saveTasks(tasks);
            std::cout << "Task " << id << " marked as " << status << ".\n";
            return;
        }
    }
    std::cerr << "Error: Task with ID " << id << " not found.\n";
}

void cmdList(const std::string& filter = "") {
    auto tasks = loadTasks();
    if (tasks.empty()) {
        std::cout << "No tasks found.\n";
        return;
    }

    bool found = false;
    std::cout << "\n";
    for (const auto& t : tasks) {
        if (!filter.empty() && t.status != filter) continue;
        found = true;
        std::cout << "[" << t.id << "] " << t.description << "\n"
            << "     Status    : " << t.status << "\n"
            << "     Created   : " << t.createdAt << "\n"
            << "     Updated   : " << t.updatedAt << "\n\n";
    }
    if (!found)
        std::cout << "No tasks with status \"" << filter << "\".\n";
}

void printUsage() {
    std::cout;
        "Usage:\n"
        "  task-cli add <description>\n"
        "  task-cli update <id> <description>\n"
        "  task-cli delete <id>\n"
        "  task-cli mark-in-progress <id>\n"
        "  task-cli mark-done <id>\n"
        "  task-cli list\n"
        "  task-cli list todo\n"
        "  task-cli list in-progress\n"
        "  task-cli list done\n";
}

// ─── Entry Point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(); return 1; }

    std::string cmd = argv[1];

    if (cmd == "add") {
        if (argc < 3) { std::cerr << "Usage: task-cli add <description>\n"; return 1; }
        cmdAdd(argv[2]);

    }
    else if (cmd == "update") {
        if (argc < 4) { std::cerr << "Usage: task-cli update <id> <description>\n"; return 1; }
        cmdUpdate(std::stoi(argv[2]), argv[3]);

    }
    else if (cmd == "delete") {
        if (argc < 3) { std::cerr << "Usage: task-cli delete <id>\n"; return 1; }
        cmdDelete(std::stoi(argv[2]));

    }
    else if (cmd == "mark-in-progress") {
        if (argc < 3) { std::cerr << "Usage: task-cli mark-in-progress <id>\n"; return 1; }
        cmdMark(std::stoi(argv[2]), "in-progress");

    }
    else if (cmd == "mark-done") {
        if (argc < 3) { std::cerr << "Usage: task-cli mark-done <id>\n"; return 1; }
        cmdMark(std::stoi(argv[2]), "done");

    }
    else if (cmd == "list") {
        std::string filter = (argc >= 3) ? argv[2] : "";
        cmdList(filter);

    }
    else {
        std::cerr << "Unknown command: " << cmd << "\n";
        printUsage();
        return 1;
    }

    return 0;
}