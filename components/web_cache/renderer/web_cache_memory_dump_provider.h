// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_MEMORY_DUMP_PROVIDER_H
#define COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_MEMORY_DUMP_PROVIDER_H

#include "base/lazy_instance.h"
#include "base/trace_event/memory_dump_provider.h"

namespace web_cache {

class WebCacheMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static WebCacheMemoryDumpProvider* GetInstance();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend struct base::DefaultLazyInstanceTraits<WebCacheMemoryDumpProvider>;

  WebCacheMemoryDumpProvider();
  ~WebCacheMemoryDumpProvider() override;

  DISALLOW_COPY_AND_ASSIGN(WebCacheMemoryDumpProvider);
};

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_MEMORY_DUMP_PROVIDER_H
