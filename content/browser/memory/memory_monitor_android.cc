// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "jni/MemoryMonitorAndroid_jni.h"

namespace content {

namespace {
const size_t kMBShift = 20;
}

// static
std::unique_ptr<MemoryMonitorAndroid> MemoryMonitorAndroid::Create() {
  return base::WrapUnique(new MemoryMonitorAndroid);
}

// static
bool MemoryMonitorAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MemoryMonitorAndroid::MemoryMonitorAndroid() {}

MemoryMonitorAndroid::~MemoryMonitorAndroid() {}

int MemoryMonitorAndroid::GetFreeMemoryUntilCriticalMB() {
  MemoryInfo info;
  GetMemoryInfo(&info);
  return (info.avail_mem - info.threshold) >> kMBShift;
}

void MemoryMonitorAndroid::GetMemoryInfo(MemoryInfo* out) {
  DCHECK(out);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MemoryMonitorAndroid_getMemoryInfo(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(out));
}

// Called by JNI to populate ActivityManager.MemoryInfo.
static void GetMemoryInfoCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz,
    jlong avail_mem,
    jboolean low_memory,
    jlong threshold,
    jlong total_mem,
    jlong out_ptr) {
  DCHECK(out_ptr);
  MemoryMonitorAndroid::MemoryInfo* info =
      reinterpret_cast<MemoryMonitorAndroid::MemoryInfo*>(out_ptr);
  info->avail_mem = avail_mem;
  info->low_memory = low_memory;
  info->threshold = threshold;
  info->total_mem = total_mem;
}

// Implementation of a factory function defined in memory_monitor.h.
std::unique_ptr<MemoryMonitor> CreateMemoryMonitor() {
  return MemoryMonitorAndroid::Create();
}

}  // namespace content
