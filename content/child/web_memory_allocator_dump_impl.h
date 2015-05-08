// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_MEMORY_ALLOCATOR_DUMP_IMPL_H_
#define CONTENT_CHILD_WEB_MEMORY_ALLOCATOR_DUMP_IMPL_H_

#include "base/memory/scoped_vector.h"
#include "third_party/WebKit/public/platform/WebMemoryAllocatorDump.h"

namespace base {
namespace trace_event {
class MemoryAllocatorDump;
}  // namespace base
}  // namespace trace_event

namespace content {

// Implements the blink::WebMemoryAllocatorDump interface by means of proxying
// the Add*() calls to the underlying base::trace_event::MemoryAllocatorDump
// instance.
class WebMemoryAllocatorDumpImpl : public blink::WebMemoryAllocatorDump {
 public:
  explicit WebMemoryAllocatorDumpImpl(
      base::trace_event::MemoryAllocatorDump* memory_allocator_dump);
  virtual ~WebMemoryAllocatorDumpImpl();

  // blink::WebMemoryAllocatorDump implementation.
  virtual void AddScalar(const blink::WebString& name,
                         const char* units,
                         uint64 value);
  virtual void AddString(const blink::WebString& name,
                         const char* units,
                         const blink::WebString& value);

 private:
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebMemoryAllocatorDumpImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_MEMORY_ALLOCATOR_DUMP_IMPL_H_
