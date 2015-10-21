// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_MEMORY_DUMP_PROVIDER_ADAPTER_H_
#define CONTENT_CHILD_WEB_MEMORY_DUMP_PROVIDER_ADAPTER_H_

#include "base/macros.h"
#include "base/trace_event/memory_dump_provider.h"

namespace blink {
class WebMemoryDumpProvider;
}  // namespace blink

namespace content {

// Adapter class which makes it possible to register a WebMemoryDumpProvider (
// from blink) to base::trace_event::MemoryDumpManager, which expects a
// MemoryDumpProvider instead.
// This is essentially proxying the OnMemoryDump from chromium base to blink.
class WebMemoryDumpProviderAdapter
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit WebMemoryDumpProviderAdapter(blink::WebMemoryDumpProvider* wmdp);
  ~WebMemoryDumpProviderAdapter() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void OnHeapProfilingEnabled(bool enabled) override;

  bool is_registered() const { return is_registered_; }
  void set_is_registered(bool is_registered) { is_registered_ = is_registered; }

 private:
  // The underlying WebMemoryDumpProvider instance to which the OnMemoryDump()
  // calls will be proxied to.
  blink::WebMemoryDumpProvider* web_memory_dump_provider_;  // Not owned.

  // True iff this has been registered with the MDM (and not unregistered yet).
  bool is_registered_;

  DISALLOW_COPY_AND_ASSIGN(WebMemoryDumpProviderAdapter);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_MEMORY_DUMP_PROVIDER_ADAPTER_H_
