// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/gatt_client_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "chromecast/device/bluetooth/shlib/mock_gatt_client.h"
#include "chromecast/device/bluetooth/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace chromecast {
namespace bluetooth {

namespace {

const bluetooth_v2_shlib::Addr kTestAddr1 = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};

class MockGattClientManagerObserver : public GattClientManager::Observer {
 public:
  MOCK_METHOD2(OnConnectChanged,
               void(scoped_refptr<RemoteDevice> device, bool connected));
  MOCK_METHOD2(OnMtuChanged, void(scoped_refptr<RemoteDevice> device, int mtu));
  MOCK_METHOD2(OnServicesUpdated,
               void(scoped_refptr<RemoteDevice> device,
                    std::vector<scoped_refptr<RemoteService>> services));
  MOCK_METHOD3(OnCharacteristicNotification,
               void(scoped_refptr<RemoteDevice> device,
                    scoped_refptr<RemoteCharacteristic> characteristic,
                    std::vector<uint8_t> value));
};

std::vector<bluetooth_v2_shlib::Gatt::Service> GenerateServices() {
  std::vector<bluetooth_v2_shlib::Gatt::Service> ret;

  bluetooth_v2_shlib::Gatt::Service service;
  bluetooth_v2_shlib::Gatt::Characteristic characteristic;
  bluetooth_v2_shlib::Gatt::Descriptor descriptor;

  service.uuid = {{0x1}};
  service.handle = 0x1;
  service.primary = true;

  characteristic.uuid = {{0x1, 0x1}};
  characteristic.handle = 0x2;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_NOTIFY;

  descriptor.uuid = {{0x1, 0x1, 0x1}};
  descriptor.handle = 0x3;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);

  descriptor.uuid = RemoteDescriptor::kCccdUuid;
  descriptor.handle = 0x4;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);
  service.characteristics.push_back(characteristic);

  characteristic.uuid = {{0x1, 0x2}};
  characteristic.handle = 0x5;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties =
      static_cast<bluetooth_v2_shlib::Gatt::Properties>(0);
  characteristic.descriptors.clear();

  ret.push_back(service);

  service.uuid = {{0x2}};
  service.handle = 0x6;
  service.primary = true;
  service.characteristics.clear();
  ret.push_back(service);

  return ret;
}

class GattClientManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    message_loop_ =
        std::make_unique<base::MessageLoop>(base::MessageLoop::TYPE_DEFAULT);
    gatt_client_ = std::make_unique<bluetooth_v2_shlib::MockGattClient>();
    gatt_client_manager_ =
        std::make_unique<GattClientManager>(gatt_client_.get());
    observer_ = std::make_unique<MockGattClientManagerObserver>();

    // Normally bluetooth_manager does this.
    gatt_client_->SetDelegate(gatt_client_manager_.get());
    gatt_client_manager_->Initialize(base::ThreadTaskRunnerHandle::Get());
    gatt_client_manager_->AddObserver(observer_.get());
  }

  void TearDown() override {
    gatt_client_->SetDelegate(nullptr);
    gatt_client_manager_->RemoveObserver(observer_.get());
    gatt_client_manager_->Finalize();
  }

  scoped_refptr<RemoteDevice> GetDevice(const bluetooth_v2_shlib::Addr& addr) {
    scoped_refptr<RemoteDevice> ret;
    gatt_client_manager_->GetDevice(
        addr, base::BindOnce(
                  [](scoped_refptr<RemoteDevice>* ret_ptr,
                     scoped_refptr<RemoteDevice> result) { *ret_ptr = result; },
                  &ret));

    return ret;
  }

  std::vector<scoped_refptr<RemoteService>> GetServices(RemoteDevice* device) {
    std::vector<scoped_refptr<RemoteService>> ret;
    device->GetServices(base::BindOnce(
        [](std::vector<scoped_refptr<RemoteService>>* ret_ptr,
           std::vector<scoped_refptr<RemoteService>> result) {
          *ret_ptr = result;
        },
        &ret));

    return ret;
  }

  scoped_refptr<RemoteService> GetServiceByUuid(
      RemoteDevice* device,
      const bluetooth_v2_shlib::Uuid& uuid) {
    scoped_refptr<RemoteService> ret;
    device->GetServiceByUuid(uuid, base::BindOnce(
                                       [](scoped_refptr<RemoteService>* ret_ptr,
                                          scoped_refptr<RemoteService> result) {
                                         *ret_ptr = result;
                                       },
                                       &ret));
    return ret;
  }

  void Connect(const bluetooth_v2_shlib::Addr& addr) {
    EXPECT_CALL(*gatt_client_, Connect(addr)).WillOnce(Return(true));
    scoped_refptr<RemoteDevice> device = GetDevice(addr);
    cb_checker_.Reset();
    device->Connect(cb_checker_.CreateCallback());
    bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
        gatt_client_->delegate();
    delegate->OnConnectChanged(addr, true /* status */, true /* connected */);
    ASSERT_TRUE(cb_checker_.called());
    ASSERT_TRUE(cb_checker_.status());
    ASSERT_TRUE(device->IsConnected());
  }

  StatusCallbackChecker cb_checker_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<GattClientManager> gatt_client_manager_;
  std::unique_ptr<bluetooth_v2_shlib::MockGattClient> gatt_client_;
  std::unique_ptr<MockGattClientManagerObserver> observer_;
};

}  // namespace

TEST_F(GattClientManagerTest, RemoteDeviceConnect) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_EQ(kTestAddr1, device->addr());

  // These should fail if we're not connected.
  cb_checker_.Reset();
  device->Disconnect(cb_checker_.CreateCallback());
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_FALSE(cb_checker_.status());

  {
    bool called = false;
    bool status = false;
    device->ReadRemoteRssi(base::BindOnce(
        [](bool* pcalled, bool* pstatus, bool status, int rssi) {
          *pcalled = true;
          *pstatus = status;
        },
        &called, &status));
    EXPECT_TRUE(called);
    EXPECT_FALSE(status);
  }

  cb_checker_.Reset();
  device->RequestMtu(512, cb_checker_.CreateCallback());
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_FALSE(cb_checker_.status());

  cb_checker_.Reset();
  device->ConnectionParameterUpdate(10, 10, 50, 100,
                                    cb_checker_.CreateCallback());
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_FALSE(cb_checker_.status());

  {
    bool called = false;
    bool status = false;
    device->DiscoverServices(base::BindOnce(
        [](bool* pstatus, bool* pcalled, bool status,
           std::vector<scoped_refptr<RemoteService>> services) {
          *pcalled = true;
          *pstatus = status;
        },
        &status, &called));
    EXPECT_TRUE(called);
    EXPECT_FALSE(status);
  }

  EXPECT_CALL(*gatt_client_, Connect(kTestAddr1)).WillOnce(Return(true));

  cb_checker_.Reset();
  device->Connect(cb_checker_.CreateCallback());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_CALL(*observer_, OnConnectChanged(device, true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_TRUE(cb_checker_.status());

  EXPECT_TRUE(device->IsConnected());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  device->Disconnect({});
  EXPECT_TRUE(device->IsConnected());

  EXPECT_CALL(*observer_, OnConnectChanged(device, false));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
  EXPECT_FALSE(device->IsConnected());
  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceReadRssi) {
  static const int kRssi = -34;

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  Connect(kTestAddr1);

  bool rssi_cb_called = false;
  auto rssi_cb = [](bool* cb_called, bool status, int rssi) {
    *cb_called = true;
    EXPECT_TRUE(status);
    EXPECT_EQ(kRssi, rssi);
  };

  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr1)).WillOnce(Return(true));
  device->ReadRemoteRssi(base::BindOnce(rssi_cb, &rssi_cb_called));

  delegate->OnReadRemoteRssi(kTestAddr1, true /* status */, kRssi);
  EXPECT_TRUE(rssi_cb_called);
}

TEST_F(GattClientManagerTest, RemoteDeviceRequestMtu) {
  static const int kMtu = 512;
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  Connect(kTestAddr1);

  EXPECT_EQ(RemoteDevice::kDefaultMtu, device->GetMtu());
  EXPECT_CALL(*gatt_client_, RequestMtu(kTestAddr1, kMtu))
      .WillOnce(Return(true));
  cb_checker_.Reset();
  device->RequestMtu(kMtu, cb_checker_.CreateCallback());
  EXPECT_CALL(*observer_, OnMtuChanged(device, kMtu));
  delegate->OnMtuChanged(kTestAddr1, true, kMtu);
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_TRUE(cb_checker_.status());
  EXPECT_EQ(kMtu, device->GetMtu());
  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceConnectionParameterUpdate) {
  const int kMinInterval = 10;
  const int kMaxInterval = 10;
  const int kLatency = 50;
  const int kTimeout = 100;

  Connect(kTestAddr1);

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_CALL(*gatt_client_,
              ConnectionParameterUpdate(kTestAddr1, kMinInterval, kMaxInterval,
                                        kLatency, kTimeout))
      .WillOnce(Return(true));
  cb_checker_.Reset();
  device->ConnectionParameterUpdate(kMinInterval, kMaxInterval, kLatency,
                                    kTimeout, cb_checker_.CreateCallback());
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_TRUE(cb_checker_.status());
}

TEST_F(GattClientManagerTest, RemoteDeviceServices) {
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_EQ(0ul, GetServices(device.get()).size());

  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(true));

  bool status = false;
  std::vector<scoped_refptr<RemoteService>> services;
  device->DiscoverServices(base::BindOnce(
      [](bool* pstatus, std::vector<scoped_refptr<RemoteService>>* pservices,
         bool status, std::vector<scoped_refptr<RemoteService>> services) {
        *pstatus = status;
        *pservices = services;
      },
      &status, &services));
  EXPECT_CALL(*observer_, OnServicesUpdated(device, _));
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(status);

  EXPECT_EQ(kServices.size(), GetServices(device.get()).size());
  for (const auto& service : kServices) {
    scoped_refptr<RemoteService> remote_service =
        GetServiceByUuid(device.get(), service.uuid);
    ASSERT_TRUE(remote_service);
    EXPECT_EQ(service.uuid, remote_service->uuid());
    EXPECT_EQ(service.handle, remote_service->handle());
    EXPECT_EQ(service.primary, remote_service->primary());
    EXPECT_EQ(service.characteristics.size(),
              remote_service->GetCharacteristics().size());

    for (const auto& characteristic : service.characteristics) {
      scoped_refptr<RemoteCharacteristic> remote_char =
          remote_service->GetCharacteristicByUuid(characteristic.uuid);
      ASSERT_TRUE(remote_char);
      EXPECT_EQ(characteristic.uuid, remote_char->uuid());
      EXPECT_EQ(characteristic.handle, remote_char->handle());
      EXPECT_EQ(characteristic.permissions, remote_char->permissions());
      EXPECT_EQ(characteristic.properties, remote_char->properties());
      EXPECT_EQ(characteristic.descriptors.size(),
                remote_char->GetDescriptors().size());

      for (const auto& descriptor : characteristic.descriptors) {
        scoped_refptr<RemoteDescriptor> remote_desc =
            remote_char->GetDescriptorByUuid(descriptor.uuid);
        ASSERT_TRUE(remote_desc);
        EXPECT_EQ(descriptor.uuid, remote_desc->uuid());
        EXPECT_EQ(descriptor.handle, remote_desc->handle());
        EXPECT_EQ(descriptor.permissions, remote_desc->permissions());
      }
    }
  }
}

TEST_F(GattClientManagerTest, RemoteDeviceCharacteristic) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const std::vector<uint8_t> kTestData2 = {0x4, 0x5, 0x6};
  const std::vector<uint8_t> kTestData3 = {0x7, 0x8, 0x9};
  const auto kServices = GenerateServices();
  const bluetooth_v2_shlib::Gatt::Client::AuthReq kAuthReq =
      bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const bluetooth_v2_shlib::Gatt::WriteType kWriteType =
      bluetooth_v2_shlib::Gatt::WRITE_TYPE_DEFAULT;

  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  auto characteristic = characteristics[0];

  EXPECT_CALL(*gatt_client_,
              WriteCharacteristic(kTestAddr1, characteristic->characteristic(),
                                  kAuthReq, kWriteType, kTestData1))
      .WillOnce(Return(true));

  bool write_cb_called = false;
  auto write_cb = base::BindOnce(
      [](bool* cb_called, bool status) {
        *cb_called = true;
        DCHECK(status);
      },
      &write_cb_called);

  characteristic->WriteAuth(kAuthReq, kWriteType, kTestData1,
                            std::move(write_cb));
  delegate->OnCharacteristicWriteResponse(kTestAddr1, true,
                                          characteristic->handle());
  EXPECT_TRUE(write_cb_called);

  EXPECT_CALL(*gatt_client_,
              ReadCharacteristic(kTestAddr1, characteristic->characteristic(),
                                 kAuthReq))
      .WillOnce(Return(true));

  bool cb_called = false;
  auto read_cb = [](bool* cb_called, const std::vector<uint8_t>& expected,
                    bool success, const std::vector<uint8_t>& value) {
    *cb_called = true;
    EXPECT_TRUE(success);
    EXPECT_EQ(expected, value);
  };
  characteristic->ReadAuth(kAuthReq,
                           base::BindOnce(read_cb, &cb_called, kTestData2));
  delegate->OnCharacteristicReadResponse(kTestAddr1, true,
                                         characteristic->handle(), kTestData2);
  EXPECT_TRUE(cb_called);

  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));

  cb_checker_.Reset();
  characteristic->SetNotification(true, cb_checker_.CreateCallback());
  EXPECT_TRUE(cb_checker_.called());
  EXPECT_TRUE(cb_checker_.status());

  EXPECT_CALL(*observer_,
              OnCharacteristicNotification(device, characteristic, kTestData3));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData3);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest,
       RemoteDeviceCharacteristicSetRegisterNotification) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  scoped_refptr<RemoteService> service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  scoped_refptr<RemoteCharacteristic> characteristic = characteristics[0];

  scoped_refptr<RemoteDescriptor> cccd =
      characteristic->GetDescriptorByUuid(RemoteDescriptor::kCccdUuid);
  ASSERT_TRUE(cccd);

  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));
  std::vector<uint8_t> cccd_enable_notification = {
      std::begin(bluetooth::RemoteDescriptor::kEnableNotificationValue),
      std::end(bluetooth::RemoteDescriptor::kEnableNotificationValue)};
  EXPECT_CALL(*gatt_client_, WriteDescriptor(kTestAddr1, cccd->descriptor(), _,
                                             cccd_enable_notification))
      .WillOnce(Return(true));

  StatusCallbackChecker cb_checker;
  characteristic->SetRegisterNotification(true, cb_checker.CreateCallback());
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, cccd->handle());
  EXPECT_TRUE(cb_checker.called());
  EXPECT_TRUE(cb_checker.status());

  EXPECT_CALL(*observer_,
              OnCharacteristicNotification(device, characteristic, kTestData1));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceDescriptor) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const std::vector<uint8_t> kTestData2 = {0x4, 0x5, 0x6};
  const bluetooth_v2_shlib::Gatt::Client::AuthReq kAuthReq =
      bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  auto characteristic = characteristics[0];

  std::vector<scoped_refptr<RemoteDescriptor>> descriptors =
      characteristic->GetDescriptors();
  ASSERT_GE(descriptors.size(), 1ul);
  auto descriptor = descriptors[0];

  EXPECT_CALL(*gatt_client_,
              WriteDescriptor(kTestAddr1, descriptor->descriptor(), kAuthReq,
                              kTestData1))
      .WillOnce(Return(true));
  bool write_cb_called = false;
  auto write_cb = base::BindOnce(
      [](bool* cb_called, bool status) {
        *cb_called = true;
        DCHECK(status);
      },
      &write_cb_called);
  descriptor->WriteAuth(kAuthReq, kTestData1, std::move(write_cb));
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, descriptor->handle());
  EXPECT_TRUE(write_cb_called);

  EXPECT_CALL(*gatt_client_,
              ReadDescriptor(kTestAddr1, descriptor->descriptor(), kAuthReq))
      .WillOnce(Return(true));

  bool cb_called = false;
  auto read_cb = [](bool* cb_called, const std::vector<uint8_t>& expected,
                    bool success, const std::vector<uint8_t>& value) {
    *cb_called = true;
    EXPECT_TRUE(success);
    EXPECT_EQ(expected, value);
  };
  descriptor->ReadAuth(kAuthReq,
                       base::BindOnce(read_cb, &cb_called, kTestData2));
  delegate->OnDescriptorReadResponse(kTestAddr1, true, descriptor->handle(),
                                     kTestData2);
  EXPECT_TRUE(cb_called);
}

}  // namespace bluetooth
}  // namespace chromecast
