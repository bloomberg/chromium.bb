// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_SERVICE_IMPL_H
#define DEVICE_VR_VR_SERVICE_IMPL_H

#include <memory>

#include "base/macros.h"

#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRServiceImpl : public VRService {
 public:
  DEVICE_VR_EXPORT ~VRServiceImpl() override;

  DEVICE_VR_EXPORT static void BindRequest(
      mojo::InterfaceRequest<VRService> request);

  VRServiceClientPtr& client() { return client_; }

 private:
  friend class VRServiceTestBinding;

  DEVICE_VR_EXPORT VRServiceImpl();

  // Mojo connection handling.
  DEVICE_VR_EXPORT void Bind(mojo::InterfaceRequest<VRService> request);
  void RemoveFromDeviceManager();

  // mojom::VRService implementation
  void SetClient(VRServiceClientPtr client) override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void GetPose(uint32_t index, const GetPoseCallback& callback) override;
  void ResetPose(uint32_t index) override;

  void RequestPresent(uint32_t index,
                      bool secureOrigin,
                      const RequestPresentCallback& callback) override;
  void ExitPresent(uint32_t index) override;
  void SubmitFrame(uint32_t index, VRPosePtr pose) override;
  void UpdateLayerBounds(uint32_t index,
                         VRLayerBoundsPtr leftBounds,
                         VRLayerBoundsPtr rightBounds) override;

  std::unique_ptr<mojo::Binding<VRService>> binding_;
  VRServiceClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace device

#endif  // DEVICE_VR_VR_SERVICE_IMPL_H
