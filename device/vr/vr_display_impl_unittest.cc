// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_display_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "device/vr/vr_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class VRDisplayImplTest : public testing::Test {
 public:
  VRDisplayImplTest() {}
  ~VRDisplayImplTest() override {}
  void onDisplaySynced() {}
  void onPresentComplete(
      bool success,
      device::mojom::VRDisplayFrameTransportOptionsPtr transport_options) {
    is_request_presenting_success_ = success;
  }

 protected:
  void SetUp() override {
    device_ = std::make_unique<FakeVRDevice>();
    mojom::VRServiceClientPtr proxy;
    client_ = std::make_unique<FakeVRServiceClient>(mojo::MakeRequest(&proxy));
  }

  std::unique_ptr<VRDisplayImpl> MakeDisplay() {
    return std::make_unique<VRDisplayImpl>(
        device(), client(), device()->GetVRDisplayInfo(), nullptr, false);
  }

  void RequestPresent(VRDisplayImpl* display_impl) {
    // TODO(klausw,mthiesse): set up a VRSubmitFrameClient here? Currently,
    // the FakeVRDisplay doesn't access the submit client, so a nullptr
    // is ok.
    device::mojom::VRSubmitFrameClientPtr submit_client = nullptr;
    device::mojom::VRPresentationProviderRequest request = nullptr;
    display_impl->RequestPresent(
        std::move(submit_client), std::move(request), nullptr,
        base::Bind(&VRDisplayImplTest::onPresentComplete,
                   base::Unretained(this)));
  }

  void ExitPresent(VRDisplayImpl* display_impl) { display_impl->ExitPresent(); }

  bool presenting() { return !!device_->GetPresentingDisplay(); }
  VRDeviceBase* device() { return device_.get(); }
  FakeVRServiceClient* client() { return client_.get(); }

  base::MessageLoop message_loop_;
  bool is_request_presenting_success_ = false;
  std::unique_ptr<FakeVRDevice> device_;
  std::unique_ptr<FakeVRServiceClient> client_;

  DISALLOW_COPY_AND_ASSIGN(VRDisplayImplTest);
};

TEST_F(VRDisplayImplTest, DevicePresentationIsolation) {
  std::unique_ptr<VRDisplayImpl> display_1 = MakeDisplay();
  std::unique_ptr<VRDisplayImpl> display_2 = MakeDisplay();

  // When not presenting either service should be able to access the device.
  EXPECT_TRUE(device()->IsAccessAllowed(display_1.get()));
  EXPECT_TRUE(device()->IsAccessAllowed(display_2.get()));

  // Attempt to present without focus.
  RequestPresent(display_1.get());
  EXPECT_FALSE(is_request_presenting_success_);
  EXPECT_FALSE(presenting());

  // Begin presenting to the fake device with service 1.
  display_1->SetInFocusedFrame(true);
  RequestPresent(display_1.get());
  EXPECT_TRUE(is_request_presenting_success_);
  EXPECT_TRUE(presenting());

  // Service 2 should not be able to present to the device while service 1
  // is still presenting.
  RequestPresent(display_2.get());
  EXPECT_FALSE(is_request_presenting_success_);
  display_2->SetInFocusedFrame(true);
  RequestPresent(display_2.get());
  EXPECT_FALSE(is_request_presenting_success_);
  EXPECT_TRUE(device()->IsAccessAllowed(display_1.get()));
  EXPECT_FALSE(device()->IsAccessAllowed(display_2.get()));

  // Service 2 should not be able to exit presentation to the device.
  ExitPresent(display_2.get());
  EXPECT_TRUE(presenting());

  // Service 1 should be able to exit the presentation it initiated.
  ExitPresent(display_1.get());
  EXPECT_FALSE(presenting());

  // Once presentation had ended both services should be able to access the
  // device.
  EXPECT_TRUE(device()->IsAccessAllowed(display_1.get()));
  EXPECT_TRUE(device()->IsAccessAllowed(display_2.get()));
}

// This test case tests that VRDisplayImpl dispatches a "vrdevicechanged" event
// to its client when it's been told the device has changed.
TEST_F(VRDisplayImplTest, DeviceChangedDispatched) {
  auto display = MakeDisplay();
  display->OnChanged(device()->GetVRDisplayInfo());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client()->CheckDeviceId(device()->GetId()));
}
}
