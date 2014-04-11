// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;
using device::BluetoothUUID;

namespace chromeos {

namespace {

const BluetoothUUID kHeartRateMeasurementUUID(
    FakeBluetoothGattCharacteristicClient::kHeartRateMeasurementUUID);
const BluetoothUUID kBodySensorLocationUUID(
    FakeBluetoothGattCharacteristicClient::kBodySensorLocationUUID);
const BluetoothUUID kHeartRateControlPointUUID(
    FakeBluetoothGattCharacteristicClient::kHeartRateControlPointUUID);

// Compares GATT characteristic/descriptor values. Returns true, if the values
// are equal.
bool ValuesEqual(const std::vector<uint8>& value0,
                 const std::vector<uint8>& value1) {
  if (value0.size() != value1.size())
    return false;
  for (size_t i = 0; i < value0.size(); ++i)
    if (value0[i] != value1[i])
      return false;
  return true;
}

class TestDeviceObserver : public BluetoothDevice::Observer {
 public:
  TestDeviceObserver(scoped_refptr<BluetoothAdapter> adapter,
                     BluetoothDevice* device)
      : gatt_service_added_count_(0),
        gatt_service_removed_count_(0),
        device_address_(device->GetAddress()),
        adapter_(adapter) {
    device->AddObserver(this);
  }

  virtual ~TestDeviceObserver() {
    BluetoothDevice* device = adapter_->GetDevice(device_address_);
    if (device)
      device->RemoveObserver(this);
  }

  // BluetoothDevice::Observer overrides.
  virtual void GattServiceAdded(
      BluetoothDevice* device,
      BluetoothGattService* service) OVERRIDE {
    ASSERT_EQ(device_address_, device->GetAddress());

    ++gatt_service_added_count_;
    last_gatt_service_id_ = service->GetIdentifier();
    last_gatt_service_uuid_ = service->GetUUID();

    EXPECT_FALSE(service->IsLocal());
    EXPECT_TRUE(service->IsPrimary());

    EXPECT_EQ(device->GetGattService(last_gatt_service_id_), service);

    QuitMessageLoop();
  }

  virtual void GattServiceRemoved(
      BluetoothDevice* device,
      BluetoothGattService* service) OVERRIDE {
    ASSERT_EQ(device_address_, device->GetAddress());

    ++gatt_service_removed_count_;
    last_gatt_service_id_ = service->GetIdentifier();
    last_gatt_service_uuid_ = service->GetUUID();

    EXPECT_FALSE(service->IsLocal());
    EXPECT_TRUE(service->IsPrimary());

    // The device should return NULL for this service.
    EXPECT_FALSE(device->GetGattService(last_gatt_service_id_));

    QuitMessageLoop();
  }

  int gatt_service_added_count_;
  int gatt_service_removed_count_;
  std::string last_gatt_service_id_;
  BluetoothUUID last_gatt_service_uuid_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  std::string device_address_;
  scoped_refptr<BluetoothAdapter> adapter_;
};

class TestGattServiceObserver : public BluetoothGattService::Observer {
 public:
  TestGattServiceObserver(scoped_refptr<BluetoothAdapter> adapter,
                          BluetoothDevice* device,
                          BluetoothGattService* service)
      : gatt_service_changed_count_(0),
        gatt_characteristic_added_count_(0),
        gatt_characteristic_removed_count_(0),
        gatt_characteristic_value_changed_count_(0),
        device_address_(device->GetAddress()),
        gatt_service_id_(service->GetIdentifier()),
        adapter_(adapter) {
    service->AddObserver(this);
  }

  virtual ~TestGattServiceObserver() {
    // See if either the device or the service even exist.
    BluetoothDevice* device = adapter_->GetDevice(device_address_);
    if (!device)
      return;

    BluetoothGattService* service = device->GetGattService(gatt_service_id_);
    if (!service)
      return;

    service->RemoveObserver(this);
  }

  // BluetoothGattService::Observer overrides.
  virtual void GattServiceChanged(BluetoothGattService* service) OVERRIDE {
    ASSERT_EQ(gatt_service_id_, service->GetIdentifier());
    ++gatt_service_changed_count_;

    QuitMessageLoop();
  }

  virtual void GattCharacteristicAdded(
      BluetoothGattService* service,
      BluetoothGattCharacteristic* characteristic) OVERRIDE {
    ASSERT_EQ(gatt_service_id_, service->GetIdentifier());

    ++gatt_characteristic_added_count_;
    last_gatt_characteristic_id_ = characteristic->GetIdentifier();
    last_gatt_characteristic_uuid_ = characteristic->GetUUID();

    EXPECT_EQ(service->GetCharacteristic(last_gatt_characteristic_id_),
              characteristic);
    EXPECT_EQ(service, characteristic->GetService());

    QuitMessageLoop();
  }

  virtual void GattCharacteristicRemoved(
      BluetoothGattService* service,
      BluetoothGattCharacteristic* characteristic) OVERRIDE {
    ASSERT_EQ(gatt_service_id_, service->GetIdentifier());

    ++gatt_characteristic_removed_count_;
    last_gatt_characteristic_id_ = characteristic->GetIdentifier();
    last_gatt_characteristic_uuid_ = characteristic->GetUUID();

    // The service should return NULL for this characteristic.
    EXPECT_FALSE(service->GetCharacteristic(last_gatt_characteristic_id_));
    EXPECT_EQ(service, characteristic->GetService());

    QuitMessageLoop();
  }

  virtual void GattCharacteristicValueChanged(
      BluetoothGattService* service,
      BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) OVERRIDE {
    ASSERT_EQ(gatt_service_id_, service->GetIdentifier());

    ++gatt_characteristic_value_changed_count_;
    last_gatt_characteristic_id_ = characteristic->GetIdentifier();
    last_gatt_characteristic_uuid_ = characteristic->GetUUID();
    last_changed_characteristic_value_ = characteristic->GetValue();

    EXPECT_EQ(service->GetCharacteristic(last_gatt_characteristic_id_),
              characteristic);

    QuitMessageLoop();
  }

  int gatt_service_changed_count_;
  int gatt_characteristic_added_count_;
  int gatt_characteristic_removed_count_;
  int gatt_characteristic_value_changed_count_;
  std::string last_gatt_characteristic_id_;
  BluetoothUUID last_gatt_characteristic_uuid_;
  std::vector<uint8> last_changed_characteristic_value_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  std::string device_address_;
  std::string gatt_service_id_;
  scoped_refptr<BluetoothAdapter> adapter_;
};

}  // namespace

class BluetoothGattChromeOSTest : public testing::Test {
 public:
  BluetoothGattChromeOSTest()
      : fake_bluetooth_gatt_service_client_(NULL),
        success_callback_count_(0),
        error_callback_count_(0) {
  }

  virtual void SetUp() {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    fake_bluetooth_gatt_service_client_ =
        new FakeBluetoothGattServiceClient;
    fake_bluetooth_gatt_characteristic_client_ =
        new FakeBluetoothGattCharacteristicClient;
    fake_bluetooth_gatt_descriptor_client_ =
        new FakeBluetoothGattDescriptorClient;
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(
            fake_bluetooth_device_client_));
    fake_dbus_thread_manager->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            fake_bluetooth_gatt_service_client_));
    fake_dbus_thread_manager->SetBluetoothGattCharacteristicClient(
        scoped_ptr<BluetoothGattCharacteristicClient>(
            fake_bluetooth_gatt_characteristic_client_));
    fake_dbus_thread_manager->SetBluetoothGattDescriptorClient(
        scoped_ptr<BluetoothGattDescriptorClient>(
            fake_bluetooth_gatt_descriptor_client_));
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    GetAdapter();

    adapter_->SetPowered(
        true,
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  void GetAdapter() {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothGattChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

  void SuccessCallback() {
    ++success_callback_count_;
  }

  void ValueCallback(const std::vector<uint8>& value) {
    ++success_callback_count_;
    last_read_value_ = value;
  }

  void ErrorCallback() {
    ++error_callback_count_;
  }

 protected:
  base::MessageLoop message_loop_;

  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  FakeBluetoothGattServiceClient* fake_bluetooth_gatt_service_client_;
  FakeBluetoothGattCharacteristicClient*
      fake_bluetooth_gatt_characteristic_client_;
  FakeBluetoothGattDescriptorClient* fake_bluetooth_gatt_descriptor_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int success_callback_count_;
  int error_callback_count_;
  std::vector<uint8> last_read_value_;
};

TEST_F(BluetoothGattChromeOSTest, GattServiceAddedAndRemoved) {
  // Create a fake LE device. We store the device pointer here because this is a
  // test. It's unsafe to do this in production as the device might get deleted.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestDeviceObserver observer(adapter_, device);
  EXPECT_EQ(0, observer.gatt_service_added_count_);
  EXPECT_EQ(0, observer.gatt_service_removed_count_);
  EXPECT_TRUE(observer.last_gatt_service_id_.empty());
  EXPECT_FALSE(observer.last_gatt_service_uuid_.IsValid());
  EXPECT_TRUE(device->GetGattServices().empty());

  // Expose the fake Heart Rate Service.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.gatt_service_added_count_);
  EXPECT_EQ(0, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));

  EXPECT_EQ(observer.last_gatt_service_uuid_, service->GetUUID());

  // Hide the service.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_gatt_service_client_->HideHeartRateService();

  EXPECT_EQ(1, observer.gatt_service_added_count_);
  EXPECT_EQ(1, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_TRUE(device->GetGattServices().empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  EXPECT_EQ(NULL, device->GetGattService(observer.last_gatt_service_id_));

  // Expose the service again.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(2, observer.gatt_service_added_count_);
  EXPECT_EQ(1, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  // The object |service| points to should have been deallocated. |device|
  // should contain a brand new instance.
  service = device->GetGattService(observer.last_gatt_service_id_);
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(observer.last_gatt_service_uuid_, service->GetUUID());

  // Remove the device. The observer should be notified of the removed service.
  // |device| becomes invalid after this.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  EXPECT_EQ(2, observer.gatt_service_added_count_);
  EXPECT_EQ(2, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);
  EXPECT_EQ(
      NULL, adapter_->GetDevice(FakeBluetoothDeviceClient::kLowEnergyAddress));
}

TEST_F(BluetoothGattChromeOSTest, GattCharacteristicAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestDeviceObserver observer(adapter_, device);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);

  TestGattServiceObserver service_observer(adapter_, device, service);
  EXPECT_EQ(0, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(0, service_observer.gatt_characteristic_added_count_);
  EXPECT_EQ(0, service_observer.gatt_characteristic_removed_count_);
  EXPECT_EQ(0, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  // 3 characteristics should appear. Only 1 of the characteristics sends
  // value changed signals. Service changed should be fired once for
  // descriptor added.
  EXPECT_EQ(4, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_added_count_);
  EXPECT_EQ(0, service_observer.gatt_characteristic_removed_count_);
  EXPECT_EQ(1, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the characteristics. 3 removed signals should be received.
  fake_bluetooth_gatt_characteristic_client_->HideHeartRateCharacteristics();
  EXPECT_EQ(8, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_added_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_removed_count_);
  EXPECT_EQ(1, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Re-expose the heart rate characteristics.
  fake_bluetooth_gatt_characteristic_client_->ExposeHeartRateCharacteristics(
      fake_bluetooth_gatt_service_client_->GetHeartRateServicePath());
  EXPECT_EQ(12, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(6, service_observer.gatt_characteristic_added_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_removed_count_);
  EXPECT_EQ(2, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the service. All characteristics should disappear.
  fake_bluetooth_gatt_service_client_->HideHeartRateService();
  EXPECT_EQ(16, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(6, service_observer.gatt_characteristic_added_count_);
  EXPECT_EQ(6, service_observer.gatt_characteristic_removed_count_);
  EXPECT_EQ(2, service_observer.gatt_characteristic_value_changed_count_);
}

TEST_F(BluetoothGattChromeOSTest, GattDescriptorAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestDeviceObserver observer(adapter_, device);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);

  TestGattServiceObserver service_observer(adapter_, device, service);
  EXPECT_EQ(0, service_observer.gatt_service_changed_count_);
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(4, service_observer.gatt_service_changed_count_);

  // Only the Heart Rate Measurement characteristic has a descriptor.
  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::kClientCharacteristicConfigurationUuid,
            descriptor->GetUUID());

  // Hide the descriptor.
  fake_bluetooth_gatt_descriptor_client_->HideDescriptor(
      dbus::ObjectPath(descriptor->GetIdentifier()));
  EXPECT_TRUE(characteristic->GetDescriptors().empty());
  EXPECT_EQ(5, service_observer.gatt_service_changed_count_);

  // Expose the descriptor again.
  fake_bluetooth_gatt_descriptor_client_->ExposeDescriptor(
      dbus::ObjectPath(characteristic->GetIdentifier()),
      FakeBluetoothGattDescriptorClient::
          kClientCharacteristicConfigurationUUID);
  EXPECT_EQ(6, service_observer.gatt_service_changed_count_);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::kClientCharacteristicConfigurationUuid,
            descriptor->GetUUID());
}

TEST_F(BluetoothGattChromeOSTest, AdapterAddedAfterGattService) {
  // This unit test tests that all remote GATT objects are created for D-Bus
  // objects that were already exposed.
  adapter_ = NULL;
  EXPECT_EQ(NULL, device::BluetoothAdapterFactory::MaybeGetAdapter().get());

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible())
    base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  // Create the adapter. This should create all the GATT objects.
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);
  EXPECT_EQ(1U, device->GetGattServices().size());

  BluetoothGattService* service = device->GetGattServices()[0];
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      service->GetUUID());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));
  EXPECT_FALSE(service->IsLocal());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kBodySensorLocationUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kHeartRateControlPointUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kHeartRateMeasurementUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  ASSERT_TRUE(descriptor);
  EXPECT_EQ(BluetoothGattDescriptor::kClientCharacteristicConfigurationUuid,
            descriptor->GetUUID());
  EXPECT_FALSE(descriptor->IsLocal());
}

TEST_F(BluetoothGattChromeOSTest, GattCharacteristicValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestDeviceObserver observer(adapter_, device);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);

  TestGattServiceObserver service_observer(adapter_, device, service);
  EXPECT_EQ(0, service_observer.gatt_characteristic_value_changed_count_);

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  // We should get an initial value changed signal from the Heart Rate
  // Measurement characteristic when it getsadded.
  EXPECT_EQ(1, service_observer.gatt_characteristic_value_changed_count_);

  // The Heart Rate Measurement characteristic should send regular
  // notifications.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(2, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_EQ(kHeartRateMeasurementUUID,
            service_observer.last_gatt_characteristic_uuid_);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateMeasurementPath().value(),
            service_observer.last_gatt_characteristic_id_);

  // Receive another notification.
  service_observer.last_gatt_characteristic_id_.clear();
  service_observer.last_gatt_characteristic_uuid_ = BluetoothUUID();
  base::MessageLoop::current()->Run();
  EXPECT_EQ(3, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_EQ(kHeartRateMeasurementUUID,
            service_observer.last_gatt_characteristic_uuid_);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateMeasurementPath().value(),
            service_observer.last_gatt_characteristic_id_);

  // Issue write request to non-writeable characteristics.
  service_observer.last_gatt_characteristic_id_.clear();
  service_observer.last_gatt_characteristic_uuid_ = BluetoothUUID();

  std::vector<uint8> write_value;
  write_value.push_back(0x01);
  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateMeasurementPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateMeasurementUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(service_observer.last_gatt_characteristic_id_.empty());
  EXPECT_FALSE(service_observer.last_gatt_characteristic_uuid_.IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_value_changed_count_);

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetBodySensorLocationPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(service_observer.last_gatt_characteristic_id_.empty());
  EXPECT_FALSE(service_observer.last_gatt_characteristic_uuid_.IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(3, service_observer.gatt_characteristic_value_changed_count_);

  // Issue write request to writeable characteristic. Writing "1" to the control
  // point characteristic will immediately change its value back to "0", hence
  // sending "ValueChanged" events twice.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateControlPointPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateControlPointUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(characteristic->GetIdentifier(),
            service_observer.last_gatt_characteristic_id_);
  EXPECT_EQ(characteristic->GetUUID(),
            service_observer.last_gatt_characteristic_uuid_);
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(5, service_observer.gatt_characteristic_value_changed_count_);

  // Issue a read request.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetBodySensorLocationPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(5, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));

  // One last value changed notification.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(6, service_observer.gatt_characteristic_value_changed_count_);
  EXPECT_EQ(kHeartRateMeasurementUUID,
            service_observer.last_gatt_characteristic_uuid_);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateMeasurementPath().value(),
            service_observer.last_gatt_characteristic_id_);
}

TEST_F(BluetoothGattChromeOSTest, GattDescriptorValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestDeviceObserver observer(adapter_, device);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);

  TestGattServiceObserver service_observer(adapter_, device, service);
  EXPECT_EQ(0, service_observer.gatt_service_changed_count_);
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(4, service_observer.gatt_service_changed_count_);

  // Only the Heart Rate Measurement characteristic has a descriptor.
  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::kClientCharacteristicConfigurationUuid,
            descriptor->GetUUID());

  std::vector<uint8> desc_value;
  desc_value.push_back(0);
  desc_value.push_back(0);
  EXPECT_TRUE(ValuesEqual(desc_value, descriptor->GetValue()));

  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_read_value_.empty());

  // Read value.
  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));

  // Write value.
  desc_value[0] = 0x03;
  descriptor->WriteRemoteDescriptor(
      desc_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_TRUE(ValuesEqual(desc_value, descriptor->GetValue()));

  // Read new value.
  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_TRUE(ValuesEqual(desc_value, descriptor->GetValue()));
}

}  // namespace chromeos
