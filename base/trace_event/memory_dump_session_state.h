// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_SESSION_STATE_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_SESSION_STATE_H_

#include <string>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_allocator_attributes_type_info.h"

namespace base {
namespace trace_event {

class BASE_EXPORT MemoryDumpSessionState
    : public RefCountedThreadSafe<MemoryDumpSessionState> {
 public:
  MemoryDumpSessionState();
  MemoryAllocatorAttributesTypeInfo allocators_attributes_type_info;

 private:
  friend class RefCountedThreadSafe<MemoryDumpSessionState>;
  ~MemoryDumpSessionState();
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_SESSION_STATE_H_
