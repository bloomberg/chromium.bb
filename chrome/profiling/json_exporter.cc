// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/json_exporter.h"

#include <map>

#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

namespace profiling {

namespace {

// Maps strings to integers for the JSON string table.
using StringTable = std::map<std::string, size_t>;

struct BacktraceNode {
  BacktraceNode(size_t sid, size_t p) : string_id(sid), parent(p) {}

  static constexpr size_t kNoParent = static_cast<size_t>(-1);

  size_t string_id;
  size_t parent;  // kNoParent indicates no parent.
};

// Used as a map kep to uniquify an allocation with a given size and stack.
// Since backtraces are uniquified, this does pointer comparisons on the
// backtrace to give a stable ordering, even if that ordering has no
// intrinsic meaning.
struct UniqueAlloc {
  UniqueAlloc(const Backtrace* bt, size_t sz) : backtrace(bt), size(sz) {}

  bool operator<(const UniqueAlloc& other) const {
    return std::tie(backtrace, size) < std::tie(other.backtrace, other.size);
  }

  const Backtrace* backtrace;
  size_t size;
};

using UniqueAllocCount = std::map<UniqueAlloc, int>;

// Trace events support different "types" of allocations which various
// subsystems annotate stuff with for other types of tracing. For our purposes
// we only have one type, and use this ID.
constexpr int kTypeId = 0;

// Writes a dummy process name entry given a PID. When we have more information
// on a process it can be filled in here. But for now the tracing tools expect
// this entry since everything is associated with a PID.
void WriteProcessName(int pid, std::ostream& out) {
  out << "{ \"pid\":" << pid << ", \"ph\":\"M\", \"name\":\"process_name\", "
      << "\"args\":{\"name\":\"Browser process\"}}";
}

// Writes the dictionary keys to preceed a "dumps" trace argument.
void WriteDumpsHeader(int pid, std::ostream& out) {
  out << "{ \"pid\":" << pid << ",";
  out << "\"ph\":\"v\",";
  out << "\"name\":\"periodic_interval\",";
  out << "\"args\":{";
  out << "\"dumps\":{\n";
}

void WriteDumpsFooter(std::ostream& out) {
  out << "}}}";  // dumps, args, event
}

// Writes the dictionary keys to preceed a "heaps_v2" trace argument inside a
// "dumps". This is "v2" heap dump format.
void WriteHeapsV2Header(std::ostream& out) {
  out << "\"level_of_detail\":\"detailed\",\n";
  out << "\"heaps_v2\": {\n";
}

// Closes the dictionaries from the WriteHeapsV2Header function above.
void WriteHeapsV2Footer(std::ostream& out) {
  out << "}";  // heaps_v2
}

void WriteMemoryMaps(
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
    std::ostream& out) {
  base::trace_event::TracedValue traced_value;
  memory_instrumentation::TracingObserver::MemoryMapsAsValueInto(maps,
                                                                 &traced_value);
  out << "\"process_mmaps\":" << traced_value.ToString();
}

// Inserts or retrieves the ID for a string in the string table.
size_t AddOrGetString(std::string str, StringTable* string_table) {
  auto result = string_table->emplace(std::move(str), string_table->size());
  // "result.first" is an iterator into the map.
  return result.first->second;
}

// Returns the index into nodes of the node to reference for this stack. That
// node will reference its parent node, etc. to allow the full stack to
// be represented.
size_t AppendBacktraceStrings(const Backtrace& backtrace,
                              std::vector<BacktraceNode>* nodes,
                              StringTable* string_table) {
  int parent = -1;
  for (const Address& addr : backtrace.addrs()) {
    static constexpr char kPcPrefix[] = "pc:";
    // std::numeric_limits<>::digits gives the number of bits in the value.
    // Dividing by 4 gives the number of hex digits needed to store the value.
    // Adding to sizeof(kPcPrefix) yields the buffer size needed including the
    // null terminator.
    static constexpr int kBufSize =
        sizeof(kPcPrefix) +
        (std::numeric_limits<decltype(addr.value)>::digits / 4);
    char buf[kBufSize];
    snprintf(buf, kBufSize, "%s%" PRIx64, kPcPrefix, addr.value);
    size_t sid = AddOrGetString(buf, string_table);
    nodes->emplace_back(sid, parent);
    parent = nodes->size() - 1;
  }
  return nodes->size() - 1;  // Last item is the end of this stack.
}

// Writes the string table which looks like:
//   "strings":[
//     {"id":123,string:"This is the string"},
//     ...
//   ]
void WriteStrings(const StringTable& string_table, std::ostream& out) {
  out << "\"strings\":[";
  bool first_time = true;
  for (const auto& string_pair : string_table) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;

    out << "{\"id\":" << string_pair.second;
    // TODO(brettw) when we have real symbols this will need escaping.
    out << ",\"string\":\"" << string_pair.first << "\"}";
  }
  out << "]";
}

// Writes the nodes array in the maps section. These are all the backtrace
// entries and are indexed by the allocator nodes arra.
//   "nodes":[
//     {"id":1, "name_sid":123, "parent":17},
//     ...
//   ]
void WriteMapNodes(const std::vector<BacktraceNode>& nodes, std::ostream& out) {
  out << "\"nodes\":[";

  for (size_t i = 0; i < nodes.size(); i++) {
    if (i != 0)
      out << ",\n";

    out << "{\"id\":" << i;
    out << ",\"name_sid\":" << nodes[i].string_id;
    if (nodes[i].parent != BacktraceNode::kNoParent)
      out << ",\"parent\":" << nodes[i].parent;
    out << "}";
  }
  out << "]";
}

// Writes the number of matching allocations array which looks like:
//   "counts":[1, 1, 2 ]
void WriteCounts(const UniqueAllocCount& alloc_counts, std::ostream& out) {
  out << "\"counts\":[";
  bool first_time = true;
  for (const auto& cur : alloc_counts) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;
    out << cur.second;
  }
  out << "]";
}

// Writes the sizes of each allocation which looks like:
//   "sizes":[32, 64, 12]
void WriteSizes(const UniqueAllocCount& alloc_counts, std::ostream& out) {
  out << "\"sizes\":[";
  bool first_time = true;
  for (const auto& cur : alloc_counts) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;
    out << cur.first.size;
  }
  out << "]";
}

// Writes the types array of integers which looks like:
//   "types":[ 0, 0, 0, ]
void WriteTypes(const UniqueAllocCount& alloc_counts, std::ostream& out) {
  out << "\"types\":[";
  for (size_t i = 0; i < alloc_counts.size(); i++) {
    if (i != 0)
      out << ",";
    out << kTypeId;
  }
  out << "]";
}

// Writes the nodes array which indexes for each allocation into the maps nodes
// array written above. It looks like:
//   "nodes":[1, 5, 10]
void WriteAllocatorNodes(const UniqueAllocCount& alloc_counts,
                         const std::map<const Backtrace*, size_t>& backtraces,
                         std::ostream& out) {
  out << "\"nodes\":[";
  bool first_time = true;
  for (const auto& cur : alloc_counts) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;
    auto found = backtraces.find(cur.first.backtrace);
    out << found->second;
  }
  out << "]";
}

}  // namespace

void ExportAllocationEventSetToJSON(
    int pid,
    const AllocationEventSet& event_set,
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
    std::ostream& out,
    std::unique_ptr<base::DictionaryValue> metadata_dict) {
  out << "{ \"traceEvents\": [";
  WriteProcessName(pid, out);
  out << ",\n";
  WriteDumpsHeader(pid, out);

  WriteMemoryMaps(maps, out);
  out << ",\n";

  WriteHeapsV2Header(out);

  // Output Heaps_V2 format version. Currently "1" is the only valid value.
  out << "\"version\": 1,\n";

  StringTable string_table;

  // We hardcode one type, "[unknown]".
  size_t type_string_id = AddOrGetString("[unknown]", &string_table);

  // Find all backtraces referenced by the set. The backtrace storage will
  // contain more stacks than we want to write out (it will refer to all
  // processes, while we're only writing one). So do those only on demand.
  //
  // The map maps backtrace keys to node IDs (computed below).
  std::map<const Backtrace*, size_t> backtraces;
  for (const auto& event : event_set)
    backtraces.emplace(event.backtrace(), 0);

  // Write each backtrace, converting the string for the stack entry to string
  // IDs. The backtrace -> node ID will be filled in at this time.
  //
  // As a future optimization, compute when a stack is a superset of another
  // one and share the common nodes.
  std::vector<BacktraceNode> nodes;
  nodes.reserve(backtraces.size() * 10);  // Guesstimate for end size.
  VLOG(1) << "Number of backtraces " << backtraces.size();
  for (auto& bt : backtraces)
    bt.second = AppendBacktraceStrings(*bt.first, &nodes, &string_table);

  // Maps section.
  out << "\"maps\": {\n";
  WriteStrings(string_table, out);
  out << ",\n";
  WriteMapNodes(nodes, out);
  out << ",\n";
  out << "\"types\":[{\"id\":" << kTypeId << ",\"name_sid\":" << type_string_id
      << "}]";
  out << "},\n";  // End of maps section.

  // Aggregate allocations. Allocations of the same size and stack get grouped.
  UniqueAllocCount alloc_counts;
  for (const auto& alloc : event_set) {
    UniqueAlloc unique_alloc(alloc.backtrace(), alloc.size());
    alloc_counts[unique_alloc]++;
  }

  // Allocators section.
  out << "\"allocators\":{\"malloc\":{\n";
  WriteCounts(alloc_counts, out);
  out << ",\n";
  WriteSizes(alloc_counts, out);
  out << ",\n";
  WriteTypes(alloc_counts, out);
  out << ",\n";
  WriteAllocatorNodes(alloc_counts, backtraces, out);
  out << "}}\n";  // End of allocators section.

  WriteHeapsV2Footer(out);
  WriteDumpsFooter(out);
  out << "]";

  // Append metadata.
  if (metadata_dict) {
    std::string metadata;
    base::JSONWriter::Write(*metadata_dict, &metadata);
    out << ",\"metadata\": " << metadata;
  }

  out << "}\n";
}

}  // namespace profiling
