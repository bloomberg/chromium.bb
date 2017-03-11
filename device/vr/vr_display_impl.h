// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DISPLAY_IMPL_H
#define DEVICE_VR_VR_DISPLAY_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRServiceImpl;

class VRDisplayImpl : public mojom::VRDisplay {
 public:
  VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service);
  ~VRDisplayImpl() override;

  mojom::VRDisplayClient* client() { return client_.get(); }

 private:
  friend class VRDisplayImplTest;
  friend class VRServiceImpl;

  void ResetPose() override;

  void RequestPresent(bool secure_origin,
                      mojom::VRSubmitFrameClientPtr submit_client,
                      const RequestPresentCallback& callback) override;
  void ExitPresent() override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;

  void UpdateLayerBounds(int16_t frame_index,
                         mojom::VRLayerBoundsPtr left_bounds,
                         mojom::VRLayerBoundsPtr right_bounds,
                         int16_t source_width,
                         int16_t source_height) override;
  void GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) override;

  void RequestPresentResult(const RequestPresentCallback& callback,
                            bool secure_origin,
                            bool success);

  void OnVRDisplayInfoCreated(mojom::VRDisplayInfoPtr display_info);

  mojo::Binding<mojom::VRDisplay> binding_;
  mojom::VRDisplayClientPtr client_;
  device::VRDevice* device_;
  VRServiceImpl* service_;

  base::WeakPtrFactory<VRDisplayImpl> weak_ptr_factory_;
};

}  // namespace device

#endif  //  DEVICE_VR_VR_DISPLAY_IMPL_H
