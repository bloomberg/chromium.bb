// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"

#include "device/vr/vr_device.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {

class VRDisplayHost;

// Browser process representation of a WebVR site session. Instantiated through
// Mojo once the user loads a page containing WebVR.
// It instantiates a VRDisplayImpl for each newly connected VRDisplay and sends
// the display's info to the render process through its connected
// mojom::VRServiceClient.
class VRServiceImpl : public device::mojom::VRService {
 public:
  VRServiceImpl(int render_frame_process_id, int render_frame_routing_id);
  ~VRServiceImpl() override;

  static void Create(int render_frame_process_id,
                     int render_frame_routing_id,
                     device::mojom::VRServiceRequest request);

  // device::mojom::VRService implementation
  // Adds this service to the VRDeviceManager.
  void SetClient(device::mojom::VRServiceClientPtr service_client,
                 SetClientCallback callback) override;

  // Tells the render process that a new VR device is available.
  void ConnectDevice(device::VRDevice* device);

 private:
  void SetListeningForActivate(bool listening) override;

  std::map<device::VRDevice*, std::unique_ptr<VRDisplayHost>> displays_;

  device::mojom::VRServiceClientPtr client_;

  const int render_frame_process_id_;
  const int render_frame_routing_id_;

  base::WeakPtrFactory<VRServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
