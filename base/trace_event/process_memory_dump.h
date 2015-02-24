// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
#define BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_

#include "base/base_export.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"

namespace base {
namespace trace_event {

class ConvertableToTraceFormat;

// ProcessMemoryDump is as a strongly typed container which enforces the data
// model for each memory dump point and holds the dumps produced by the
// MemoryDumpProvider(s) for a specific process.
// At trace generation time (i.e. when AsValue() is called), ProcessMemoryDump
// will compose a key-value dictionary of the various dumps obtained at trace
// dump point time.
class BASE_EXPORT ProcessMemoryDump {
 public:
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

 private:
  ProcessMemoryTotals process_totals_;
  bool has_process_totals_;

  ProcessMemoryMaps process_mmaps_;
  bool has_process_mmaps_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryDump);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
