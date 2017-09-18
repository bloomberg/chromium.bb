// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_SERVICE_IMPL_H
#define DEVICE_VR_VR_SERVICE_IMPL_H

#include <memory>

#include "base/macros.h"

#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRDisplayImpl;

// Browser process representation of a WebVR site session. Instantiated through
// Mojo once the user loads a page containing WebVR.
// It instantiates a VRDisplayImpl for each newly connected VRDisplay and sends
// the display's info to the render process through its connected
// mojom::VRServiceClient.
class VRServiceImpl : public mojom::VRService {
 public:
  DEVICE_VR_EXPORT VRServiceImpl(int render_frame_process_id,
                                 int render_frame_routing_id);
  ~VRServiceImpl() override;

  DEVICE_VR_EXPORT static void Create(int render_frame_process_id,
                                      int render_frame_routing_id,
                                      mojom::VRServiceRequest request);

  // mojom::VRService implementation
  // Adds this service to the VRDeviceManager.
  void SetClient(mojom::VRServiceClientPtr service_client,
                 SetClientCallback callback) override;

  // Tells the render process that a new VR device is available.
  void ConnectDevice(VRDevice* device);

 private:
  friend class VRDisplayImplTest;

  void SetListeningForActivate(bool listening) override;

  std::map<VRDevice*, std::unique_ptr<VRDisplayImpl>> displays_;

  mojom::VRServiceClientPtr client_;

  const int render_frame_process_id_;
  const int render_frame_routing_id_;

  base::WeakPtrFactory<VRServiceImpl> weak_ptr_factory_;

  // Getter for testing.
  DEVICE_VR_EXPORT VRDisplayImpl* GetVRDisplayImplForTesting(VRDevice* device);

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace device

#endif  // DEVICE_VR_VR_SERVICE_IMPL_H
