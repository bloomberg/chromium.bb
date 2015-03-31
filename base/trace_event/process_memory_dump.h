// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
#define BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_

#include "base/base_export.h"
#include "base/containers/hash_tables.h"
#include "base/containers/small_map.h"
#include "base/memory/scoped_vector.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"

namespace base {
namespace trace_event {

class ConvertableToTraceFormat;
class MemoryDumpManager;

// ProcessMemoryDump is as a strongly typed container which enforces the data
// model for each memory dump and holds the dumps produced by the
// MemoryDumpProvider(s) for a specific process.
// At trace generation time (i.e. when AsValue() is called), ProcessMemoryDump
// will compose a key-value dictionary of the various dumps obtained at trace
// dump point time.
class BASE_EXPORT ProcessMemoryDump {
 public:
  using AllocatorDumpsMap =
      SmallMap<hash_map<std::string, MemoryAllocatorDump*>>;

  ProcessMemoryDump();
  ~ProcessMemoryDump();

  // Called at trace generation time to populate the TracedValue.
  void AsValueInto(TracedValue* value) const;

  ProcessMemoryTotals* process_totals() { return &process_totals_; }
  bool has_process_totals() const { return has_process_totals_; }
  void set_has_process_totals() { has_process_totals_ = true; }

  ProcessMemoryMaps* process_mmaps() { return &process_mmaps_; }
  bool has_process_mmaps() const { return has_process_mmaps_; }
  void set_has_process_mmaps() { has_process_mmaps_ = true; }

  // Creates a new MemoryAllocatorDump with the given name and returns the
  // empty object back to the caller. The |name| must be unique in the dump.
  // ProcessMemoryDump handles the memory ownership of the created object.
  // |parent| can be used to specify a hierarchical relationship of the
  // allocator dumps.
  MemoryAllocatorDump* CreateAllocatorDump(const std::string& name);
  MemoryAllocatorDump* CreateAllocatorDump(const std::string& name,
                                           MemoryAllocatorDump* parent);

  // Returns a MemoryAllocatorDump given its name or nullptr if not found.
  MemoryAllocatorDump* GetAllocatorDump(const std::string& name) const;

  // Returns the map of the MemoryAllocatorDumps added to this dump.
  const AllocatorDumpsMap& allocator_dumps() const { return allocator_dumps_; }

 private:
  ProcessMemoryTotals process_totals_;
  bool has_process_totals_;

  ProcessMemoryMaps process_mmaps_;
  bool has_process_mmaps_;

  // A maps of "allocator_name" -> MemoryAllocatorDump populated by
  // allocator dump providers.
  AllocatorDumpsMap allocator_dumps_;

  // ProcessMemoryDump handles the memory ownership of all its belongings.
  ScopedVector<MemoryAllocatorDump> allocator_dumps_storage_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryDump);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
