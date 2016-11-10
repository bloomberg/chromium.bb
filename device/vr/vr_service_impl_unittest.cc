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

  MOCK_METHOD1(OnExitPresent, void(uint32_t index));

  MOCK_METHOD1(OnDisplayConnected, void(const VRDisplay& display));
  void OnDisplayConnected(VRDisplayPtr display) override {
    OnDisplayConnected(*display);
    last_display_ = std::move(display);
  }
  void OnDisplayDisconnected(unsigned index) override {}

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
  VRServiceImpl* service() { return service_impl_.get(); }

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

  bool presenting() { return !!device_manager_->presenting_service_; }

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

// Ensure that presenting devices cannot be accessed by other services
TEST_F(VRServiceImplTest, DevicePresentationIsolation) {
  std::unique_ptr<VRServiceTestBinding> service_1 = BindService();
  std::unique_ptr<VRServiceTestBinding> service_2 = BindService();

  std::unique_ptr<FakeVRDevice> device(new FakeVRDevice(provider_));
  provider_->AddDevice(device.get());

  // Ensure the device manager has seen the fake device
  device_manager_->GetVRDevices();

  // When not presenting either service should be able to access the device
  EXPECT_EQ(device.get(), VRDeviceManager::GetAllowedDevice(
                              service_1->service(), device->id()));
  EXPECT_EQ(device.get(), VRDeviceManager::GetAllowedDevice(
                              service_2->service(), device->id()));

  // Begin presenting to the fake device with service 1
  EXPECT_TRUE(device_manager_->RequestPresent(service_1->service(),
                                              device->id(), true));

  EXPECT_TRUE(presenting());

  // Service 2 should not be able to present to the device while service 1
  // is still presenting.
  EXPECT_FALSE(device_manager_->RequestPresent(service_2->service(),
                                               device->id(), true));

  // Only the presenting service should be able to access the device
  EXPECT_EQ(device.get(), VRDeviceManager::GetAllowedDevice(
                              service_1->service(), device->id()));
  EXPECT_EQ(nullptr, VRDeviceManager::GetAllowedDevice(service_2->service(),
                                                       device->id()));

  // Service 2 should not be able to exit presentation to the device
  device_manager_->ExitPresent(service_2->service(), device->id());
  EXPECT_TRUE(presenting());

  // Service 1 should be able to exit the presentation it initiated.
  device_manager_->ExitPresent(service_1->service(), device->id());
  EXPECT_FALSE(presenting());

  // Once presention had ended both services should be able to access the device
  EXPECT_EQ(device.get(), VRDeviceManager::GetAllowedDevice(
                              service_1->service(), device->id()));
  EXPECT_EQ(device.get(), VRDeviceManager::GetAllowedDevice(
                              service_2->service(), device->id()));
}

// Ensure that DeviceChanged calls are dispatched to all active services.
TEST_F(VRServiceImplTest, DeviceConnectedDispatched) {
  std::unique_ptr<VRServiceTestBinding> service_1 = BindService();
  std::unique_ptr<VRServiceTestBinding> service_2 = BindService();

  EXPECT_CALL(service_1->client(), OnDisplayConnected(_));
  EXPECT_CALL(service_2->client(), OnDisplayConnected(_));

  std::unique_ptr<FakeVRDevice> device(new FakeVRDevice(provider_));
  device_manager_->OnDeviceConnectionStatusChanged(device.get(), true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(device->id(), service_1->client().LastDisplay()->index);
  EXPECT_EQ(device->id(), service_2->client().LastDisplay()->index);
}
}
