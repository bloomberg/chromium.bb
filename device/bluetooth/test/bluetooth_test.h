// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapter;
class BluetoothDevice;

// A test fixture for Bluetooth that abstracts platform specifics for creating
// and controlling fake low level objects.
//
// Subclasses on each platform implement this, and are then typedef-ed to
// BluetoothTest.
class BluetoothTestBase : public testing::Test {
 public:
  static const std::string kTestAdapterName;
  static const std::string kTestAdapterAddress;

  static const std::string kTestDeviceName;
  static const std::string kTestDeviceNameEmpty;

  static const std::string kTestDeviceAddress1;
  static const std::string kTestDeviceAddress2;

  static const std::string kTestUUIDGenericAccess;
  static const std::string kTestUUIDGenericAttribute;
  static const std::string kTestUUIDImmediateAlert;
  static const std::string kTestUUIDLinkLoss;

  BluetoothTestBase();
  ~BluetoothTestBase() override;

  // Calls adapter_->StartDiscoverySessionWithFilter with Low Energy transport,
  // and this fixture's callbacks. Then RunLoop().RunUntilIdle().
  void StartLowEnergyDiscoverySession();

  // Check if Low Energy is available. On Mac, we require OS X >= 10.10.
  virtual bool PlatformSupportsLowEnergy() = 0;

  // Initializes the BluetoothAdapter |adapter_| with the system adapter.
  virtual void InitWithDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with the system adapter forced
  // to be ignored as if it did not exist. This enables tests for when an
  // adapter is not present on the system.
  virtual void InitWithoutDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with a fake adapter that can be
  // controlled by this test fixture.
  virtual void InitWithFakeAdapter(){};

  // Create a fake Low Energy device and discover it.
  // |device_ordinal| selects between multiple fake device data sets to produce:
  //   1: kTestDeviceName with advertised UUIDs kTestUUIDGenericAccess,
  //      kTestUUIDGenericAttribute and address kTestDeviceAddress1.
  //   2: kTestDeviceName with advertised UUIDs kTestUUIDImmediateAlert,
  //      kTestUUIDLinkLoss and address kTestDeviceAddress1.
  //   3: kTestDeviceNameEmpty with no advertised UUIDs and address
  //      kTestDeviceAddress1.
  //   4: kTestDeviceNameEmpty with no advertised UUIDs and address
  //      kTestDeviceAddress2.
  virtual BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal);

  // Simulates success of implementation details of CreateGattConnection.
  virtual void SimulateGattConnection(BluetoothDevice* device) {}

  // Simulates failure of CreateGattConnection with the given error code.
  virtual void SimulateGattConnectionError(BluetoothDevice* device,
                                           BluetoothDevice::ConnectErrorCode) {}

  // Simulates GattConnection disconnecting.
  virtual void SimulateGattDisconnection(BluetoothDevice* device) {}

  // Simulates success of discovering services. |uuids| is used to create a
  // service for each UUID string. Multiple UUIDs with the same value produce
  // multiple service instances.
  virtual void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids) {}

  // Simulates failure to discover services.
  virtual void SimulateGattServicesDiscoveryError(BluetoothDevice* device) {}

  // Simulates a Characteristic on a service.
  virtual void SimulateGattCharacteristic(BluetoothGattService* service,
                                          const std::string& uuid,
                                          int properties) {}

  // Simulates a Characteristic Set Notify success.
  virtual void SimulateGattNotifySessionStarted(
      BluetoothGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Set Notify operation failing synchronously once
  // for an unknown reason.
  virtual void SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Read operation succeeding, returning |value|.
  virtual void SimulateGattCharacteristicRead(
      BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) {}

  // Simulates a Characteristic Read operation failing with a GattErrorCode.
  virtual void SimulateGattCharacteristicReadError(
      BluetoothGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Characteristic Read operation failing synchronously once for an
  // unknown reason.
  virtual void SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Write operation succeeding, returning |value|.
  virtual void SimulateGattCharacteristicWrite(
      BluetoothGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Write operation failing with a GattErrorCode.
  virtual void SimulateGattCharacteristicWriteError(
      BluetoothGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Characteristic Write operation failing synchronously once for
  // an unknown reason.
  virtual void SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) {}

  // Remove the device from the adapter and delete it.
  virtual void DeleteDevice(BluetoothDevice* device);

  // Callbacks that increment |callback_count_|, |error_callback_count_|:
  void Callback();
  void DiscoverySessionCallback(scoped_ptr<BluetoothDiscoverySession>);
  void GattConnectionCallback(scoped_ptr<BluetoothGattConnection>);
  void NotifyCallback(scoped_ptr<BluetoothGattNotifySession>);
  void ReadValueCallback(const std::vector<uint8>& value);
  void ErrorCallback();
  void ConnectErrorCallback(enum BluetoothDevice::ConnectErrorCode);
  void GattErrorCallback(BluetoothGattService::GattErrorCode);

  // Accessors to get callbacks bound to this fixture:
  base::Closure GetCallback();
  BluetoothAdapter::DiscoverySessionCallback GetDiscoverySessionCallback();
  BluetoothDevice::GattConnectionCallback GetGattConnectionCallback();
  BluetoothGattCharacteristic::NotifySessionCallback GetNotifyCallback();
  BluetoothGattCharacteristic::ValueCallback GetReadValueCallback();
  BluetoothAdapter::ErrorCallback GetErrorCallback();
  BluetoothDevice::ConnectErrorCallback GetConnectErrorCallback();
  base::Callback<void(BluetoothGattService::GattErrorCode)>
  GetGattErrorCallback();

  // Reset all event count members to 0.
  void ResetEventCounts();

  // A Message loop is required by some implementations that will PostTasks and
  // by base::RunLoop().RunUntilIdle() use in this fixuture.
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;
  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;
  ScopedVector<BluetoothGattConnection> gatt_connections_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_code_ =
      BluetoothDevice::ERROR_UNKNOWN;
  ScopedVector<BluetoothGattNotifySession> notify_sessions_;
  std::vector<uint8> last_read_value_;
  std::vector<uint8> last_write_value_;
  BluetoothGattService::GattErrorCode last_gatt_error_code_;
  int callback_count_ = 0;
  int error_callback_count_ = 0;
  int gatt_connection_attempts_ = 0;
  int gatt_disconnection_attempts_ = 0;
  int gatt_discovery_attempts_ = 0;
  int gatt_notify_characteristic_attempts_ = 0;
  int gatt_read_characteristic_attempts_ = 0;
  int gatt_write_characteristic_attempts_ = 0;
  base::WeakPtrFactory<BluetoothTestBase> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
