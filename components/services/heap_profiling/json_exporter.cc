// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/json_exporter.h"

#include <map>
#include <unordered_map>

#include "base/containers/adapters.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

namespace heap_profiling {

namespace {

// Maps strings to integers for the JSON string table.
using StringTable = std::unordered_map<std::string, size_t>;

// Maps allocation site to node_id of the top frame.
using AllocationToNodeId = std::unordered_map<const AllocationSite*, size_t>;

constexpr uint32_t kAllocatorCount =
    static_cast<uint32_t>(AllocatorType::kMaxValue) + 1;

struct BacktraceNode {
  BacktraceNode(size_t string_id, size_t parent)
      : string_id_(string_id), parent_(parent) {}

  static constexpr size_t kNoParent = static_cast<size_t>(-1);

  size_t string_id() const { return string_id_; }
  size_t parent() const { return parent_; }

  bool operator<(const BacktraceNode& other) const {
    return std::tie(string_id_, parent_) <
           std::tie(other.string_id_, other.parent_);
  }

 private:
  const size_t string_id_;
  const size_t parent_;  // kNoParent indicates no parent.
};

using BacktraceTable = std::map<BacktraceNode, size_t>;

// The hardcoded ID for having no context for an allocation.
constexpr int kUnknownTypeId = 0;

const char* StringForAllocatorType(uint32_t type) {
  switch (static_cast<AllocatorType>(type)) {
    case AllocatorType::kMalloc:
      return "malloc";
    case AllocatorType::kPartitionAlloc:
      return "partition_alloc";
    case AllocatorType::kOilpan:
      return "blink_gc";
    default:
      NOTREACHED();
      return "unknown";
  }
}

// Writes the top-level allocators section. This section is used by the tracing
// UI to show a small summary for each allocator. It's necessary as a
// placeholder to allow the stack-viewing UI to be shown.
void WriteAllocatorsSummary(const AllocationMap& allocations,
                            std::ostream& out) {
  // Aggregate stats for each allocator type.
  size_t total_size[kAllocatorCount] = {0};
  size_t total_count[kAllocatorCount] = {0};
  for (const auto& alloc_pair : allocations) {
    uint32_t index = static_cast<uint32_t>(alloc_pair.first.allocator);
    total_size[index] += alloc_pair.second.size;
    total_count[index] += alloc_pair.second.count;
  }

  out << "\"allocators\":{\n";
  for (uint32_t i = 0; i < kAllocatorCount; i++) {
    const char* alloc_type = StringForAllocatorType(i);

    // Overall sizes.
    static constexpr char kAttrsSizeBody[] = R"(
      "%s": {
        "attrs": {
          "virtual_size": {
            "type": "scalar",
            "units": "bytes",
            "value": "%zx"
          },
          "size": {
            "type": "scalar",
            "units": "bytes",
            "value": "%zx"
          }
        }
      },)";
    out << base::StringPrintf(kAttrsSizeBody, alloc_type, total_size[i],
                              total_size[i]);

    // Allocated objects.
    static constexpr char kAttrsObjectsBody[] = R"(
      "%s/allocated_objects": {
        "attrs": {
          "shim_allocated_objects_count": {
            "type": "scalar",
            "units": "objects",
            "value": "%zx"
          },
          "shim_allocated_objects_size": {
            "type": "scalar",
            "units": "bytes",
            "value": "%zx"
          }
        }
      })";
    out << base::StringPrintf(kAttrsObjectsBody, alloc_type, total_count[i],
                              total_size[i]);

    // Comma except for the last time.
    if (i < kAllocatorCount - 1)
      out << ',';
    out << "\n";
  }
  out << "},\n";
}

// Writes the dictionary keys to preceed a "heaps_v2" trace argument inside a
// "dumps". This is "v2" heap dump format.
void WriteHeapsV2Header(std::ostream& out) {
  out << "\"heaps_v2\": {\n";
}

// Closes the dictionaries from the WriteHeapsV2Header function above.
void WriteHeapsV2Footer(std::ostream& out) {
  out << "}";  // heaps_v2
}

void WriteMemoryMaps(const ExportParams& params, std::ostream& out) {
  base::trace_event::TracedValue traced_value(0, /*force_json=*/true);
  memory_instrumentation::TracingObserver::MemoryMapsAsValueInto(
      params.maps, &traced_value, params.strip_path_from_mapped_files);
  out << "\"process_mmaps\":" << traced_value.ToString();
}

// Inserts or retrieves the ID for a string in the string table.
size_t AddOrGetString(const std::string& str,
                      StringTable* string_table,
                      ExportParams* params) {
  return string_table->emplace(str, params->next_id++).first->second;
}

// Processes the context information.
// Strings are added for each referenced context and a mapping between
// context IDs and string IDs is filled in for each.
void FillContextStrings(ExportParams* params,
                        StringTable* string_table,
                        std::map<int, size_t>* context_to_string_id_map) {
  // The context map is backwards from what we need, so iterate through the
  // whole thing and see which ones are used.
  for (const auto& context : params->context_map) {
    size_t string_id = AddOrGetString(context.first, string_table, params);
    context_to_string_id_map->emplace(context.second, string_id);
  }

  // Hard code a string for the unknown context type.
  context_to_string_id_map->emplace(
      kUnknownTypeId, AddOrGetString("[unknown]", string_table, params));
}

size_t AddOrGetBacktraceNode(BacktraceNode node,
                             BacktraceTable* backtrace_table,
                             ExportParams* params) {
  return backtrace_table->emplace(std::move(node), params->next_id++)
      .first->second;
}

// Returns the index into nodes of the node to reference for this stack. That
// node will reference its parent node, etc. to allow the full stack to
// be represented.
size_t AppendBacktraceStrings(const AllocationSite& alloc,
                              BacktraceTable* backtrace_table,
                              StringTable* string_table,
                              ExportParams* params) {
  size_t parent = BacktraceNode::kNoParent;
  // Addresses must be outputted in reverse order.
  for (const Address addr : base::Reversed(alloc.stack)) {
    size_t sid;
    auto it = params->mapped_strings.find(addr);
    if (it != params->mapped_strings.end()) {
      sid = AddOrGetString(it->second, string_table, params);
    } else {
      static constexpr char kPcPrefix[] = "pc:";
      // std::numeric_limits<>::digits gives the number of bits in the value.
      // Dividing by 4 gives the number of hex digits needed to store the value.
      // Adding to sizeof(kPcPrefix) yields the buffer size needed including the
      // null terminator.
      static constexpr int kBufSize =
          sizeof(kPcPrefix) + std::numeric_limits<decltype(addr)>::digits / 4;
      char buf[kBufSize];
      snprintf(buf, kBufSize, "%s%" PRIx64, kPcPrefix, addr);
      sid = AddOrGetString(buf, string_table, params);
    }
    parent = AddOrGetBacktraceNode(BacktraceNode(sid, parent), backtrace_table,
                                   params);
  }
  return parent;  // Last item is the end of this stack.
}

// Writes the string table which looks like:
//   "strings":[
//     {"id":123,"string":"This is the string"},
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
// entries and are indexed by the allocator nodes array.
//   "nodes":[
//     {"id":1, "name_sid":123, "parent":17},
//     ...
//   ]
void WriteMapNodes(const BacktraceTable& nodes, std::ostream& out) {
  out << "\"nodes\":[";

  bool first_time = true;
  for (const auto& node : nodes) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;

    out << "{\"id\":" << node.second;
    out << ",\"name_sid\":" << node.first.string_id();
    if (node.first.parent() != BacktraceNode::kNoParent)
      out << ",\"parent\":" << node.first.parent();
    out << "}";
  }
  out << "]";
}

// Write the types array in the maps section. These are the context for each
// allocation and just maps IDs to string IDs.
//    "types":[
//      {"id":1, "name_sid":123},
//    ]
void WriteTypeNodes(const std::map<int, size_t>& type_to_string,
                    std::ostream& out) {
  out << "\"types\":[";

  bool first_time = true;
  for (const auto& type : type_to_string) {
    if (!first_time)
      out << ",\n";
    else
      first_time = false;

    out << "{\"id\":" << type.first << ",\"name_sid\":" << type.second << "}";
  }
  out << "]";
}

// Writes the number of matching allocations array which looks like:
//   "counts":[1, 1, 2]
void WriteCounts(const AllocationMap& allocations,
                 int allocator,
                 std::ostream& out) {
  out << "\"counts\":[";
  bool first_time = true;
  for (const auto& cur : allocations) {
    if (static_cast<int>(cur.first.allocator) != allocator)
      continue;
    if (!first_time)
      out << ",";
    else
      first_time = false;
    out << cur.second.count;
  }
  out << "]";
}

// Writes the total sizes of each allocation which looks like:
//   "sizes":[32, 64, 12]
void WriteSizes(const AllocationMap& allocations,
                int allocator,
                std::ostream& out) {
  out << "\"sizes\":[";
  bool first_time = true;
  for (const auto& cur : allocations) {
    if (static_cast<int>(cur.first.allocator) != allocator)
      continue;
    if (!first_time)
      out << ",";
    else
      first_time = false;
    out << cur.second.size;
  }
  out << "]";
}

// Writes the types array of integers which looks like:
//   "types":[0, 0, 1]
void WriteTypes(const AllocationMap& allocations,
                int allocator,
                std::ostream& out) {
  out << "\"types\":[";
  bool first_time = true;
  for (const auto& cur : allocations) {
    if (static_cast<int>(cur.first.allocator) != allocator)
      continue;
    if (!first_time)
      out << ",";
    else
      first_time = false;
    out << cur.first.context_id;
  }
  out << "]";
}

// Writes the nodes array which indexes for each allocation into the maps nodes
// array written above. It looks like:
//   "nodes":[1, 5, 10]
void WriteAllocatorNodes(const AllocationMap& allocations,
                         int allocator,
                         const AllocationToNodeId& alloc_to_node_id,
                         std::ostream& out) {
  out << "\"nodes\":[";
  bool first_time = true;
  for (const auto& cur : allocations) {
    if (static_cast<int>(cur.first.allocator) != allocator)
      continue;
    if (!first_time)
      out << ",";
    else
      first_time = false;
    out << alloc_to_node_id.at(&cur.first);
  }
  out << "]";
}

}  // namespace

ExportParams::ExportParams() = default;
ExportParams::~ExportParams() = default;

void ExportMemoryMapsAndV2StackTraceToJSON(ExportParams* params,
                                           std::ostream& out) {
  // Start dictionary.
  out << "{\n";

  WriteMemoryMaps(*params, out);
  out << ",\n";

  // Write level of detail.
  out << R"("level_of_detail": "detailed")"
      << ",\n";

  WriteAllocatorsSummary(params->allocs, out);
  WriteHeapsV2Header(out);

  // Output Heaps_V2 format version. Currently "1" is the only valid value.
  out << "\"version\": 1,\n";

  // Put all required context strings in the string table and generate a
  // mapping from allocation context_id to string ID.
  StringTable string_table;
  std::map<int, size_t> context_to_string_id_map;
  FillContextStrings(params, &string_table, &context_to_string_id_map);

  AllocationToNodeId alloc_to_node_id;
  BacktraceTable nodes;
  // For each backtrace, converting the string for the stack entry to string
  // IDs. The backtrace -> node ID will be filled in at this time.
  for (const auto& alloc : params->allocs) {
    size_t node_id =
        AppendBacktraceStrings(alloc.first, &nodes, &string_table, params);
    alloc_to_node_id.emplace(&alloc.first, node_id);
  }

  // Maps section.
  out << "\"maps\": {\n";
  WriteStrings(string_table, out);
  out << ",\n";
  WriteMapNodes(nodes, out);
  out << ",\n";
  WriteTypeNodes(context_to_string_id_map, out);
  out << "},\n";  // End of maps section.

  // Allocators section.
  out << "\"allocators\":{\n";
  for (uint32_t i = 0; i < kAllocatorCount; i++) {
    out << "  \"" << StringForAllocatorType(i) << "\":{\n    ";

    WriteCounts(params->allocs, i, out);
    out << ",\n    ";
    WriteSizes(params->allocs, i, out);
    out << ",\n    ";
    WriteTypes(params->allocs, i, out);
    out << ",\n    ";
    WriteAllocatorNodes(params->allocs, i, alloc_to_node_id, out);
    out << "\n  }";

    // Comma every time but the last.
    if (i < kAllocatorCount - 1)
      out << ',';
    out << "\n";
  }
  out << "}\n";  // End of allocators section.

  WriteHeapsV2Footer(out);

  // End dictionary.
  out << "}\n";
}

}  // namespace heap_profiling
