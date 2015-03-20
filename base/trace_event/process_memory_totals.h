// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_PROCESS_MEMORY_TOTALS_H_
#define BASE_TRACE_EVENT_PROCESS_MEMORY_TOTALS_H_

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {
namespace trace_event {

class TracedValue;

// Data model for process-wide memory stats.
class BASE_EXPORT ProcessMemoryTotals {
 public:
  ProcessMemoryTotals() : resident_set_bytes_(0) {}

  // Called at trace generation time to populate the TracedValue.
  void AsValueInto(TracedValue* value) const;

  uint64 resident_set_bytes() const { return resident_set_bytes_; }
  void set_resident_set_bytes(uint64 value) { resident_set_bytes_ = value; }

 private:
  uint64 resident_set_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryTotals);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_PROCESS_MEMORY_TOTALS_H_
