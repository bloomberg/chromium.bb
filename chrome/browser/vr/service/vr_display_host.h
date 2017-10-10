// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_
#define CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_

#include <memory>

#include "base/macros.h"

#include "device/vr/vr_device.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {
class VRDevice;
class VRDisplayImpl;
}  // namespace device

namespace vr {

// The browser-side host for a device::VRDisplayImpl. Controls access to VR
// APIs like poses and presentation.
// TODO(mthiesse, crbug.com/768923): Move focus code from VrShellDelegate to
// here.
class VRDisplayHost : public device::mojom::VRDisplayHost {
 public:
  VRDisplayHost(device::VRDevice* device,
                int render_frame_process_id,
                int render_frame_routing_id,
                device::mojom::VRServiceClient* service_client,
                device::mojom::VRDisplayInfoPtr display_info);
  ~VRDisplayHost() override;

  void RequestPresent(device::mojom::VRSubmitFrameClientPtr client,
                      device::mojom::VRPresentationProviderRequest request,
                      RequestPresentCallback callback) override;
  void ExitPresent() override;
  void SetListeningForActivate(bool listening);

 private:
  std::unique_ptr<device::VRDisplayImpl> display_;

  mojo::Binding<device::mojom::VRDisplayHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(VRDisplayHost);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_
