// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "device/vr/test/mock_vr_display_impl.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

void DoNothing(bool will_not_present) {}

class VRDeviceBaseForTesting : public VRDeviceBase {
 public:
  VRDeviceBaseForTesting() = default;
  ~VRDeviceBaseForTesting() override = default;

  void SetVRDisplayInfoForTest(mojom::VRDisplayInfoPtr display_info) {
    SetVRDisplayInfo(std::move(display_info));
  }

  void FireDisplayActivate() {
    OnActivate(device::mojom::VRDisplayEventReason::MOUNTED,
               base::Bind(&DoNothing));
  }

  bool ListeningForActivate() { return listening_for_activate; }

 private:
  void OnListeningForActivate(bool listening) override {
    listening_for_activate = listening;
  }

  bool listening_for_activate = false;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBaseForTesting);
};

}  // namespace

class VRDeviceTest : public testing::Test {
 public:
  VRDeviceTest() {}
  ~VRDeviceTest() override {}

 protected:
  void SetUp() override {
    mojom::VRServiceClientPtr proxy;
    client_ = std::make_unique<FakeVRServiceClient>(mojo::MakeRequest(&proxy));
  }

  std::unique_ptr<MockVRDisplayImpl> MakeMockDisplay(VRDeviceBase* device) {
    return std::make_unique<testing::NiceMock<MockVRDisplayImpl>>(
        device, client(), nullptr, nullptr, false);
  }

  std::unique_ptr<VRDeviceBaseForTesting> MakeVRDevice() {
    std::unique_ptr<VRDeviceBaseForTesting> device =
        std::make_unique<VRDeviceBaseForTesting>();
    device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
    return device;
  }

  mojom::VRDisplayInfoPtr MakeVRDisplayInfo(unsigned int device_id) {
    mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
    display_info->index = device_id;
    return display_info;
  }

  FakeVRServiceClient* client() { return client_.get(); }

  std::unique_ptr<FakeVRServiceClient> client_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceTest);
};

// Tests VRDevice class default behaviour when it dispatches "vrdevicechanged"
// event. The expected behaviour is all of the services related with this device
// will receive the "vrdevicechanged" event.
TEST_F(VRDeviceTest, DeviceChangedDispatched) {
  auto device = MakeVRDevice();
  auto display = MakeMockDisplay(device.get());
  EXPECT_CALL(*display, DoOnChanged(testing::_)).Times(1);
  device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
}

TEST_F(VRDeviceTest, DisplayActivateRegsitered) {
  device::mojom::VRDisplayEventReason mounted =
      device::mojom::VRDisplayEventReason::MOUNTED;
  auto device = MakeVRDevice();
  auto display1 = MakeMockDisplay(device.get());
  auto display2 = MakeMockDisplay(device.get());

  EXPECT_CALL(*display1, ListeningForActivate())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*display1, InFocusedFrame())
      .WillRepeatedly(testing::Return(false));
  device->OnListeningForActivateChanged(display1.get());
  EXPECT_FALSE(device->ListeningForActivate());

  EXPECT_CALL(*display1, OnActivate(mounted, testing::_)).Times(0);
  EXPECT_CALL(*display2, OnActivate(mounted, testing::_)).Times(0);
  device->FireDisplayActivate();

  EXPECT_CALL(*display1, InFocusedFrame())
      .WillRepeatedly(testing::Return(true));
  device->OnFrameFocusChanged(display1.get());
  EXPECT_TRUE(device->ListeningForActivate());

  EXPECT_CALL(*display1, OnActivate(mounted, testing::_)).Times(1);
  device->FireDisplayActivate();

  EXPECT_CALL(*display2, ListeningForActivate())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*display2, InFocusedFrame())
      .WillRepeatedly(testing::Return(true));
  device->OnListeningForActivateChanged(display2.get());
  EXPECT_TRUE(device->ListeningForActivate());

  EXPECT_CALL(*display2, OnActivate(mounted, testing::_)).Times(3);
  device->FireDisplayActivate();

  EXPECT_CALL(*display1, ListeningForActivate())
      .WillRepeatedly(testing::Return(false));
  device->OnListeningForActivateChanged(display1.get());
  EXPECT_TRUE(device->ListeningForActivate());

  device->FireDisplayActivate();

  EXPECT_CALL(*display2, ListeningForActivate())
      .WillRepeatedly(testing::Return(false));
  device->OnListeningForActivateChanged(display2.get());
  EXPECT_FALSE(device->ListeningForActivate());

  // Even though the device says it's not listening for activate, we still send
  // it the activation to handle raciness on Android.
  device->FireDisplayActivate();

  EXPECT_CALL(*display2, InFocusedFrame())
      .WillRepeatedly(testing::Return(false));
  device->OnFrameFocusChanged(display2.get());

  // Now we no longer fire the activation.
  device->FireDisplayActivate();
}

}  // namespace device
