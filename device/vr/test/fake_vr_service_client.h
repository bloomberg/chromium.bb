// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_
#define DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
class FakeVRDisplayImplClient;

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT FakeVRServiceClient : public mojom::VRServiceClient {
 public:
  FakeVRServiceClient(mojom::VRServiceClientRequest request);
  ~FakeVRServiceClient() override;

  void OnDisplayConnected(mojom::XRDevicePtr device,
                          mojom::VRDisplayClientRequest request,
                          mojom::VRDisplayInfoPtr displayInfo) override;
  void SetLastDeviceId(mojom::XRDeviceId id);
  bool CheckDeviceId(mojom::XRDeviceId id);

 private:
  std::vector<mojom::VRDisplayInfoPtr> displays_;
  std::vector<std::unique_ptr<FakeVRDisplayImplClient>> display_clients_;
  mojom::XRDeviceId last_device_id_ = static_cast<mojom::XRDeviceId>(0);
  mojo::Binding<mojom::VRServiceClient> m_binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeVRServiceClient);
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_
