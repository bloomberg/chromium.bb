// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_
#define CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_

#include <vector>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"

namespace content {

class DevToolsGpuAgent;
class GpuCommandBufferStub;

class GpuEventsDispatcher : public base::NonThreadSafe {
 public:
  enum EventPhase {
    kEventStart,
    kEventFinish
  };

  GpuEventsDispatcher();
  ~GpuEventsDispatcher();

  void AddProcessor(DevToolsGpuAgent* processor);
  void RemoveProcessor(DevToolsGpuAgent* processor);

  static void FireEvent(EventPhase phase, GpuCommandBufferStub* stub) {
    if (!IsEnabled())
      return;
    DoFireEvent(phase, stub);
  }

private:
  static bool IsEnabled() { return enabled_; }
  static void DoFireEvent(EventPhase, GpuCommandBufferStub* stub);

  static bool enabled_;
  std::vector<DevToolsGpuAgent*> processors_;
};

namespace devtools_gpu_instrumentation {

class ScopedGpuTask {
 public:
  explicit ScopedGpuTask(GpuCommandBufferStub* stub) :
      stub_(stub) {
    GpuEventsDispatcher::FireEvent(GpuEventsDispatcher::kEventStart, stub_);
  }
  ~ScopedGpuTask() {
    GpuEventsDispatcher::FireEvent(GpuEventsDispatcher::kEventFinish, stub_);
  }
 private:
  GpuCommandBufferStub* stub_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGpuTask);
};

} // namespace devtools_gpu_instrumentation

} // namespace content

#endif  // CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_
