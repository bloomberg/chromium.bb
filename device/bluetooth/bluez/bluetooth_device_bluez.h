// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"

namespace device {
class BluetoothSocketThread;
}  // namespace device

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothPairingBlueZ;

// The BluetoothDeviceBlueZ class implements BluetoothDevice for platforms using
// BlueZ.
//
// This class is not thread-safe, but is only called from the UI thread.
//
// A socket thread is used to create sockets but posts all callbacks on the UI
// thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceBlueZ
    : public device::BluetoothDevice,
      public bluez::BluetoothGattServiceClient::Observer {
 public:
  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDList GetUUIDs() const override;
  int16_t GetInquiryRSSI() const override;
  int16_t GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ConnectToService(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;
  void Pair(device::BluetoothDevice::PairingDelegate* pairing_delegate,
            const base::Closure& callback,
            const ConnectErrorCallback& error_callback) override;

  // Creates a pairing object with the given delegate |pairing_delegate| and
  // establishes it as the pairing context for this device. All pairing-related
  // method calls will be forwarded to this object until it is released.
  BluetoothPairingBlueZ* BeginPairing(
      BluetoothDevice::PairingDelegate* pairing_delegate);

  // Releases the current pairing object, any pairing-related method calls will
  // be ignored.
  void EndPairing();

  // Returns the current pairing object or NULL if no pairing is in progress.
  BluetoothPairingBlueZ* GetPairing() const;

  // Returns the object path of the device.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Returns the adapter which owns this device instance.
  BluetoothAdapterBlueZ* adapter() const;

 protected:
  // BluetoothDevice override
  std::string GetDeviceName() const override;
  void CreateGattConnectionImpl() override;
  void DisconnectGatt() override;

 private:
  friend class BluetoothAdapterBlueZ;

  BluetoothDeviceBlueZ(
      BluetoothAdapterBlueZ* adapter,
      const dbus::ObjectPath& object_path,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);
  ~BluetoothDeviceBlueZ() override;

  // bluez::BluetoothGattServiceClient::Observer overrides.
  void GattServiceAdded(const dbus::ObjectPath& object_path) override;
  void GattServiceRemoved(const dbus::ObjectPath& object_path) override;

  // Called by dbus:: on completion of the D-Bus method call to get the
  // connection attributes of the current connection to the device.
  void OnGetConnInfo(const ConnectionInfoCallback& callback,
                     int16_t rssi,
                     int16_t transmit_power,
                     int16_t max_transmit_power);
  void OnGetConnInfoError(const ConnectionInfoCallback& callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Internal method to initiate a connection to this device, and methods called
  // by dbus:: on completion of the D-Bus method call.
  void ConnectInternal(bool after_pairing,
                       const base::Closure& callback,
                       const ConnectErrorCallback& error_callback);
  void OnConnect(bool after_pairing, const base::Closure& callback);
  void OnCreateGattConnection(const GattConnectionCallback& callback);
  void OnConnectError(bool after_pairing,
                      const ConnectErrorCallback& error_callback,
                      const std::string& error_name,
                      const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to pair the device,
  // made inside |Connect()|.
  void OnPairDuringConnect(const base::Closure& callback,
                           const ConnectErrorCallback& error_callback);
  void OnPairDuringConnectError(const ConnectErrorCallback& error_callback,
                                const std::string& error_name,
                                const std::string& error_message);

  // Called by dbus: on completion of the D-Bus method call to pair the device,
  // made inside |Pair()|.
  void OnPair(const base::Closure& callback);
  void OnPairError(const ConnectErrorCallback& error_callback,
                   const std::string& error_name,
                   const std::string& error_message);

  // Called by dbus:: on failure of the D-Bus method call to cancel pairing,
  // there is no matching completion call since we don't do anything special
  // in that case.
  void OnCancelPairingError(const std::string& error_name,
                            const std::string& error_message);

  // Internal method to set the device as trusted. Trusted devices can connect
  // to us automatically, and we can connect to them after rebooting; it also
  // causes the device to be remembered by the stack even if not paired.
  // |success| to the callback indicates whether or not the request succeeded.
  void SetTrusted();
  void OnSetTrusted(bool success);

  // Called by dbus:: on completion of the D-Bus method call to disconnect the
  // device.
  void OnDisconnect(const base::Closure& callback);
  void OnDisconnectError(const ErrorCallback& error_callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // Called by dbus:: on failure of the D-Bus method call to unpair the device;
  // there is no matching completion call since this object is deleted in the
  // process of unpairing.
  void OnForgetError(const ErrorCallback& error_callback,
                     const std::string& error_name,
                     const std::string& error_message);

  // The dbus object path of the device object.
  dbus::ObjectPath object_path_;

  // Number of ongoing calls to Connect().
  int num_connecting_calls_;

  // True if the connection monitor has been started, tracking the connection
  // RSSI and TX power.
  bool connection_monitor_started_;

  // UI thread task runner and socket thread object used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // During pairing this is set to an object that we don't own, but on which
  // we can make method calls to request, display or confirm PIN Codes and
  // Passkeys. Generally it is the object that owns this one.
  std::unique_ptr<BluetoothPairingBlueZ> pairing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_
