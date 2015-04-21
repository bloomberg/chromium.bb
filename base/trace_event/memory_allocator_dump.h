// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_
#define BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/trace_event/memory_allocator_attributes_type_info.h"
#include "base/values.h"

namespace base {
namespace trace_event {

class MemoryDumpManager;
class ProcessMemoryDump;
class TracedValue;

// Data model for user-land memory allocator dumps.
class BASE_EXPORT MemoryAllocatorDump {
 public:
  // Returns the absolute name for a given (|allocator_name|,|heap_name|) tuple.
  static std::string GetAbsoluteName(const std::string& allocator_name,
                                     const std::string& heap_name);

  // Use as argument for |heap_name| when the allocator has only one root heap.
  static const char kRootHeap[];

  // MemoryAllocatorDump is owned by ProcessMemoryDump.
  MemoryAllocatorDump(const std::string& allocator_name,
                      const std::string& heap_name,
                      ProcessMemoryDump* process_memory_dump);
  ~MemoryAllocatorDump();

  // Name of the allocator, a plain string with no separators (e.g, "malloc").
  const std::string& allocator_name() const { return allocator_name_; }

  // Name of the heap being dumped, either: "heap", "heap/subheap" or kRootHeap
  // if the allocator has just one root heap.
  const std::string& heap_name() const { return heap_name_; }

  // Absolute name, unique within the scope of an entire ProcessMemoryDump.
  // In practice this is "allocator_name/heap/subheap".
  std::string GetAbsoluteName() const;

  // Inner size: Bytes requested by clients of the allocator, without accounting
  // for any metadata or allocator-specific bookeeping structs.
  void set_allocated_objects_size_in_bytes(uint64 value) {
    allocated_objects_size_in_bytes_ = value;
  }
  uint64 allocated_objects_size_in_bytes() const {
    return allocated_objects_size_in_bytes_;
  }

  // Outer size: bytes requested to the system to handle all the allocations,
  // including any allocator-internal metadata / bookeeping structs. For
  // instance, in the case of an allocator which gets pages to the system via
  // mmap() or similar, this is the number of requested pages * 4k.
  void set_physical_size_in_bytes(uint64 value) {
    physical_size_in_bytes_ = value;
  }
  uint64 physical_size_in_bytes() const { return physical_size_in_bytes_; }

  // Number of objects allocated, if known, or 0 if not available.
  void set_allocated_objects_count(uint64 value) {
    allocated_objects_count_ = value;
  }
  uint64 allocated_objects_count() const { return allocated_objects_count_; }

  // Get/Set extra attributes. The attributes name must have been previously
  // declared through MemoryDumpProvider.DeclareAllocatorAttribute().
  void SetAttribute(const std::string& name, int value);
  int GetIntegerAttribute(const std::string& name) const;

  // Called at trace generation time to populate the TracedValue.
  void AsValueInto(TracedValue* value) const;

  // Get the ProcessMemoryDump instance that owns this.
  ProcessMemoryDump* process_memory_dump() const {
    return process_memory_dump_;
  }

  // Retrieves the map of allocator attributes types, which is shared by all
  // MemoryAllocatorDump(s) across all ProcessMemoryDump(s) per tracing session.
  const MemoryAllocatorAttributesTypeInfo& GetAttributesTypeInfo() const;

 private:
  const std::string allocator_name_;
  const std::string heap_name_;
  ProcessMemoryDump* const process_memory_dump_;  // Not owned (PMD owns this).
  uint64 physical_size_in_bytes_;
  uint64 allocated_objects_count_;
  uint64 allocated_objects_size_in_bytes_;
  DictionaryValue attributes_values_;

  DISALLOW_COPY_AND_ASSIGN(MemoryAllocatorDump);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_
