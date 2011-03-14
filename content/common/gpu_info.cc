// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu_info.h"

GPUInfo::GPUInfo()
    : finalized(false),
      vendor_id(0),
      device_id(0),
      can_lose_context(false) {
}

GPUInfo::~GPUInfo() { }
