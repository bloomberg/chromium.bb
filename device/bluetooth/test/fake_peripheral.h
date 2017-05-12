// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_PERIPHERAL_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_PERIPHERAL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/fake_central.h"

namespace bluetooth {

// Implements device::BluetoothDevice. Meant to be used by FakeCentral
// to keep track of the peripheral's state and attributes.
class FakePeripheral : public device::BluetoothDevice {
 public:
  FakePeripheral(FakeCentral* fake_central, const std::string& address);
  ~FakePeripheral() override;

  // Changes the name of the device.
  void SetName(base::Optional<std::string> name);

  // Set it to indicate if the Peripheral is connected or not.
  void SetGattConnected(bool gatt_connected);

  // Updates the peripheral's UUIDs that are returned by
  // BluetoothDevice::GetUUIDs().
  void SetServiceUUIDs(UUIDSet service_uuids);

  // BluetoothDevice overrides:
  uint32_t GetBluetoothClass() const override;
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  device::BluetoothTransport GetType() const override;
#endif
  std::string GetIdentifier() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  base::Optional<std::string> GetName() const override;
  base::string16 GetNameForDisplay() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(PairingDelegate* pairing_delegate,
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

 protected:
  void CreateGattConnectionImpl() override;
  void DisconnectGatt() override;

 private:
  const std::string address_;
  base::Optional<std::string> name_;
  bool gatt_connected_;
  UUIDSet service_uuids_;

  DISALLOW_COPY_AND_ASSIGN(FakePeripheral);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_PERIPHERAL_H_
