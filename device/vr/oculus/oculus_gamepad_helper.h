// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_
#define DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_

#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"

namespace device {

class OculusGamepadHelper {
 public:
  static mojom::XRGamepadDataPtr GetGamepadData(ovrSession session);
  static base::Optional<Gamepad> CreateGamepad(ovrSession session,
                                               ovrHandType hand);
};

}  // namespace device

#endif  // DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_
