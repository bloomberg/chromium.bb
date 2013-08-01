// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info.h"

namespace gpu {

GPUInfo::GPUDevice::GPUDevice()
    : vendor_id(0),
      device_id(0) {
}

GPUInfo::GPUDevice::~GPUDevice() { }

GPUInfo::GPUInfo()
    : finalized(false),
      optimus(false),
      amd_switchable(false),
      lenovo_dcute(false),
      adapter_luid(0),
      gl_reset_notification_strategy(0),
      can_lose_context(false),
      software_rendering(false),
      sandboxed(false) {
}

GPUInfo::~GPUInfo() { }

}  // namespace gpu
