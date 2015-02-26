// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_

#include "base/base_export.h"
#include "base/macros.h"

namespace base {
namespace trace_event {

class ProcessMemoryDump;

// The contract interface that memory dump providers must implement.
class BASE_EXPORT MemoryDumpProvider {
 public:
  // Called by the MemoryDumpManager when generating dump points.
  // Returns: true if the |pmd| was successfully populated, false otherwise.
  virtual bool DumpInto(ProcessMemoryDump* pmd) = 0;

  virtual const char* GetFriendlyName() const = 0;

 protected:
  MemoryDumpProvider() {}
  virtual ~MemoryDumpProvider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
