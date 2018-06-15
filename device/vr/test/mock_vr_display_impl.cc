// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/mock_vr_display_impl.h"

namespace device {

MockVRDisplayImpl::MockVRDisplayImpl(device::VRDevice* device,
                                     mojom::VRServiceClient* service_client,
                                     mojom::VRDisplayInfoPtr display_info,
                                     mojom::VRDisplayHostPtr display_host,
                                     mojom::VRDisplayClientRequest request,
                                     int render_process_id,
                                     int render_frame_id,
                                     bool in_frame_focused)
    : VRDisplayImpl(device,
                    std::move(service_client),
                    std::move(display_info),
                    std::move(display_host),
                    std::move(request),
                    render_process_id,
                    render_frame_id) {
  SetFrameDataRestricted(!in_frame_focused);
}

MockVRDisplayImpl::~MockVRDisplayImpl() = default;

}  // namespace device
