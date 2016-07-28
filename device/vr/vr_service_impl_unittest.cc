// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_service_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Mock;

namespace device {

class MockVRServiceClient : public VRServiceClient {
 public:
  MOCK_METHOD1(OnDisplayChanged, void(const VRDisplay& display));
  void OnDisplayChanged(VRDisplayPtr display) override {
    OnDisplayChanged(*display);
    last_display_ = std::move(display);
  }

  const VRDisplayPtr& LastDisplay() { return last_display_; }

 private:
  VRDisplayPtr last_display_;
};

class VRServiceTestBinding {
 public:
  VRServiceTestBinding() {
    auto request = mojo::GetProxy(&service_ptr_);
    service_impl_.reset(new VRServiceImpl());
    service_impl_->Bind(std::move(request));

    VRServiceClientPtr client_ptr;
    client_binding_.reset(new mojo::Binding<VRServiceClient>(
        &mock_client_, mojo::GetProxy(&client_ptr)));
    service_impl_->SetClient(std::move(client_ptr));
  }

  void Close() {
    service_ptr_.reset();
    service_impl_.reset();
  }

  MockVRServiceClient& client() { return mock_client_; }

 private:
  std::unique_ptr<VRServiceImpl> service_impl_;
  mojo::InterfacePtr<VRService> service_ptr_;

  MockVRServiceClient mock_client_;
  std::unique_ptr<mojo::Binding<VRServiceClient>> client_binding_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceTestBinding);
};

class VRServiceImplTest : public testing::Test {
 public:
  VRServiceImplTest() {}
  ~VRServiceImplTest() override {}

 protected:
  void SetUp() override {
    std::unique_ptr<FakeVRDeviceProvider> provider(new FakeVRDeviceProvider());
    provider_ = provider.get();
    device_manager_.reset(new VRDeviceManager(std::move(provider)));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<VRServiceTestBinding> BindService() {
    return std::unique_ptr<VRServiceTestBinding>(new VRServiceTestBinding());
  }

  size_t ServiceCount() { return device_manager_->services_.size(); }

  base::MessageLoop message_loop_;
  FakeVRDeviceProvider* provider_;
  std::unique_ptr<VRDeviceManager> device_manager_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceImplTest);
};

// Ensure that services are registered with the device manager as they are
// created and removed from the device manager as their connections are closed.
TEST_F(VRServiceImplTest, DeviceManagerRegistration) {
  EXPECT_EQ(0u, ServiceCount());

  std::unique_ptr<VRServiceTestBinding> service_1 = BindService();

  EXPECT_EQ(1u, ServiceCount());

  std::unique_ptr<VRServiceTestBinding> service_2 = BindService();

  EXPECT_EQ(2u, ServiceCount());

  service_1->Close();

  EXPECT_EQ(1u, ServiceCount());

  service_2->Close();

  EXPECT_EQ(0u, ServiceCount());
}

// Ensure that DeviceChanged calls are dispatched to all active services.
TEST_F(VRServiceImplTest, DeviceChangedDispatched) {
  std::unique_ptr<VRServiceTestBinding> service_1 = BindService();
  std::unique_ptr<VRServiceTestBinding> service_2 = BindService();

  EXPECT_CALL(service_1->client(), OnDisplayChanged(_));
  EXPECT_CALL(service_2->client(), OnDisplayChanged(_));

  std::unique_ptr<FakeVRDevice> device(new FakeVRDevice(provider_));
  device_manager_->OnDeviceChanged(device->GetVRDevice());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(device->id(), service_1->client().LastDisplay()->index);
  EXPECT_EQ(device->id(), service_2->client().LastDisplay()->index);
}
}
