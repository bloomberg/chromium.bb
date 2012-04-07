// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/gpu_info.h"

namespace content {

GPUInfo::GPUInfo()
    : finalized(false),
      optimus(false),
      amd_switchable(false),
      vendor_id(0),
      device_id(0),
      can_lose_context(false),
      gpu_accessible(true),
      software_rendering(false) {
}

GPUInfo::~GPUInfo() { }

}  // namespace content
