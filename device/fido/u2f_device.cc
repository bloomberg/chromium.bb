// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_device.h"

namespace device {

constexpr base::TimeDelta U2fDevice::kDeviceTimeout;

U2fDevice::U2fDevice() = default;
U2fDevice::~U2fDevice() = default;

}  // namespace device
