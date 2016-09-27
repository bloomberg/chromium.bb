// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_ANDROID_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_ANDROID_H_

#include <jni.h>

#include "content/browser/memory/memory_monitor.h"

namespace content {

// A memory monitor for the Android system.
class CONTENT_EXPORT MemoryMonitorAndroid : public MemoryMonitor {
 public:
  static std::unique_ptr<MemoryMonitorAndroid> Create();

  static bool Register(JNIEnv* env);

  // C++ counter-part of ActivityManager.MemoryInfo
  struct MemoryInfo {
    jlong avail_mem;
    jboolean low_memory;
    jlong threshold;
    jlong total_mem;
  };

  ~MemoryMonitorAndroid() override;

  // MemoryMonitor implementation:
  int GetFreeMemoryUntilCriticalMB() override;

  // Get memory info from the Android system.
  void GetMemoryInfo(MemoryInfo* out);

 private:
  MemoryMonitorAndroid();

  DISALLOW_COPY_AND_ASSIGN(MemoryMonitorAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_ANDROID_H_
