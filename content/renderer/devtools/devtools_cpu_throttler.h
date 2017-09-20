// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CPU_THROTTLER_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CPU_THROTTLER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
};

namespace content {

class CPUThrottlingThread;

// Singleton that manages creation of the throttler thread.
class CONTENT_EXPORT DevToolsCPUThrottler final {
 public:
  void SetThrottlingRate(double rate);
  static DevToolsCPUThrottler* GetInstance();

 private:
  DevToolsCPUThrottler();
  ~DevToolsCPUThrottler();
  friend struct base::DefaultSingletonTraits<DevToolsCPUThrottler>;

  std::unique_ptr<CPUThrottlingThread> throttling_thread_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsCPUThrottler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CPU_THROTTLER_H_
