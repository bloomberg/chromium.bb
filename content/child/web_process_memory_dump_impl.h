// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
#define CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebProcessMemoryDump.h"

namespace base {
class DiscardableMemory;
namespace trace_event {
class MemoryAllocatorDump;
class ProcessMemoryDump;
}  // namespace base
}  // namespace trace_event

namespace skia {
class SkiaTraceMemoryDumpImpl;
}  // namespace skia

namespace content {

class WebMemoryAllocatorDumpImpl;

// Implements the blink::WebProcessMemoryDump interface by means of proxying the
// calls to createMemoryAllocatorDump() to the underlying
// base::trace_event::ProcessMemoryDump instance.
class CONTENT_EXPORT WebProcessMemoryDumpImpl final
    : public NON_EXPORTED_BASE(blink::WebProcessMemoryDump) {
 public:
  // Creates a standalone WebProcessMemoryDumpImp, which owns the underlying
  // ProcessMemoryDump.
  WebProcessMemoryDumpImpl();

  // Wraps (without owning) an existing ProcessMemoryDump.
  explicit WebProcessMemoryDumpImpl(
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  ~WebProcessMemoryDumpImpl() override;

  // blink::WebProcessMemoryDump implementation.
  blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name) override;
  blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name,
      blink::WebMemoryAllocatorDumpGuid guid) override;
  blink::WebMemoryAllocatorDump* getMemoryAllocatorDump(
      const blink::WebString& absolute_name) const override;
  void clear() override;
  void takeAllDumpsFrom(blink::WebProcessMemoryDump* other) override;
  void addOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                        blink::WebMemoryAllocatorDumpGuid target,
                        int importance) override;
  void addOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                        blink::WebMemoryAllocatorDumpGuid target) override;
  void addSuballocation(blink::WebMemoryAllocatorDumpGuid source,
                        const blink::WebString& target_node_name) override;
  SkTraceMemoryDump* createDumpAdapterForSkia(
      const blink::WebString& dump_name_prefix) override;

  const base::trace_event::ProcessMemoryDump* process_memory_dump() const {
    return process_memory_dump_;
  }

  blink::WebMemoryAllocatorDump* CreateDiscardableMemoryAllocatorDump(
      const std::string& name,
      base::DiscardableMemory* discardable);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebProcessMemoryDumpImplTest, IntegrationTest);

  blink::WebMemoryAllocatorDump* createWebMemoryAllocatorDump(
      base::trace_event::MemoryAllocatorDump* memory_allocator_dump);

  // Only for the case of ProcessMemoryDump being owned (i.e. the default ctor).
  scoped_ptr<base::trace_event::ProcessMemoryDump> owned_process_memory_dump_;

  // The underlying ProcessMemoryDump instance to which the
  // createMemoryAllocatorDump() calls will be proxied to.
  base::trace_event::ProcessMemoryDump* process_memory_dump_;  // Not owned.

  // TODO(ssid): Remove it once this information is added to ProcessMemoryDump.
  base::trace_event::MemoryDumpLevelOfDetail level_of_detail_;

  // Reverse index of MemoryAllocatorDump -> WebMemoryAllocatorDumpImpl wrapper.
  // By design WebMemoryDumpProvider(s) are not supposed to hold the pointer
  // to the WebProcessMemoryDump passed as argument of the onMemoryDump() call.
  // Those pointers are valid only within the scope of the call and can be
  // safely torn down once the WebProcessMemoryDumpImpl itself is destroyed.
  base::ScopedPtrHashMap<base::trace_event::MemoryAllocatorDump*,
                         scoped_ptr<WebMemoryAllocatorDumpImpl>>
      memory_allocator_dumps_;

  // Stores SkTraceMemoryDump for the current ProcessMemoryDump.
  ScopedVector<skia::SkiaTraceMemoryDumpImpl> sk_trace_dump_list_;

  DISALLOW_COPY_AND_ASSIGN(WebProcessMemoryDumpImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
