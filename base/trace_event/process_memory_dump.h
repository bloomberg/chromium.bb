// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
#define BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/trace_event/trace_event_impl.h"

namespace base {
namespace trace_event {

// A container which holds the dumps produced by the MemoryDumpProvider(s)
// for a specific process. ProcessMemoryDump is as a strongly typed container
// which enforces the data model for each memory dump point.
// At trace generation time (i.e. when AppendAsTraceFormat is called) the
// ProcessMemoryDump will compose a key-value dictionary of the various dumps
// obtained during at trace dump point time.
class BASE_EXPORT ProcessMemoryDump : public ConvertableToTraceFormat {
 public:
  ProcessMemoryDump();

  // ConvertableToTraceFormat implementation.
  void AppendAsTraceFormat(std::string* out) const override;

 private:
  ~ProcessMemoryDump() override;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryDump);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_PROCESS_MEMORY_DUMP_H_
