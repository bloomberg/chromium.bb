// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/mock_vr_display_impl.h"

namespace device {

MockVRDisplayImpl::MockVRDisplayImpl(
    device::VRDeviceBase* device,
    mojom::VRMagicWindowProviderRequest session,
    mojom::XRSessionControllerRequest controller,
    bool is_frame_focused)
    : VRDisplayImpl(device, std::move(session), std::move(controller)) {
  SetFrameDataRestricted(!is_frame_focused);
}

MockVRDisplayImpl::~MockVRDisplayImpl() = default;

}  // namespace device
