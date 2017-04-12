// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_
#define DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_

#include "device/vibration/vibration_export.h"
#include "device/vibration/vibration_manager.mojom.h"

namespace device {

class VibrationManagerImpl {
 public:
  DEVICE_VIBRATION_EXPORT static void Create(
      mojo::InterfaceRequest<mojom::VibrationManager> request);

  DEVICE_VIBRATION_EXPORT static int64_t milli_seconds_for_testing_;
  DEVICE_VIBRATION_EXPORT static bool cancelled_for_testing_;
};

}  // namespace device

#endif  // DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_
