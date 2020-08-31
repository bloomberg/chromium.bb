// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/device_perf_info.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace gpu {

namespace {
// Global instance in browser process.
base::Optional<DevicePerfInfo> g_device_perf_info;

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}
}  // namespace

base::Optional<DevicePerfInfo> GetDevicePerfInfo() {
  base::AutoLock lock(GetLock());
  return g_device_perf_info;
}

void SetDevicePerfInfo(const DevicePerfInfo& device_perf_info) {
  base::AutoLock lock(GetLock());
  g_device_perf_info = device_perf_info;
}

}  // namespace gpu
