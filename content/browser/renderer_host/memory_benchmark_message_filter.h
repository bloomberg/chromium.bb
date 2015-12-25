// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEMORY_BENCHMARK_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEMORY_BENCHMARK_MESSAGE_FILTER_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class MemoryBenchmarkMessageFilter : public BrowserMessageFilter {
 public:
  MemoryBenchmarkMessageFilter();

  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~MemoryBenchmarkMessageFilter() override;

  void OnHeapProfilerDump(const std::string& reason);

  DISALLOW_COPY_AND_ASSIGN(MemoryBenchmarkMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEMORY_BENCHMARK_MESSAGE_FILTER_H_
