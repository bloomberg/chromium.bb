// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
#define CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_

#include "base/memory/scoped_vector.h"
#include "third_party/WebKit/public/platform/WebProcessMemoryDump.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace base
}  // namespace trace_event

namespace content {

class WebMemoryAllocatorDumpImpl;

// Implements the blink::WebProcessMemoryDump interface by means of proxying the
// calls to createMemoryAllocatorDump() to the underlying
// base::trace_event::ProcessMemoryDump instance.
class WebProcessMemoryDumpImpl : public blink::WebProcessMemoryDump {
 public:
  explicit WebProcessMemoryDumpImpl(
      base::trace_event::ProcessMemoryDump* process_memory_dump);
  virtual ~WebProcessMemoryDumpImpl();

  // blink::WebProcessMemoryDump implementation.
  virtual blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name);

 private:
  // The underlying ProcessMemoryDump instance to which the
  // createMemoryAllocatorDump() calls will be proxied to.
  base::trace_event::ProcessMemoryDump* process_memory_dump_;  // Not owned.

  // By design WebMemoryDumpProvider(s) are not supposed to hold the pointer
  // to the WebProcessMemoryDump passed as argument of the onMemoryDump() call.
  // Those pointers are valid only within the scope of the call and can be
  // safely torn down once the WebProcessMemoryDumpImpl itself is destroyed.
  ScopedVector<WebMemoryAllocatorDumpImpl> memory_allocator_dumps_;

  DISALLOW_COPY_AND_ASSIGN(WebProcessMemoryDumpImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
