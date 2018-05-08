// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterWinrt : public BluetoothAdapter {
 public:
  // BluetoothAdapter:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

 protected:
  // BluetoothAdapter:
  bool SetPoweredImpl(bool powered) override;
  void AddDiscoverySession(
      BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      DiscoverySessionErrorCallback error_callback) override;
  void RemoveDiscoverySession(
      BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      DiscoverySessionErrorCallback error_callback) override;
  void SetDiscoveryFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const base::Closure& callback,
      DiscoverySessionErrorCallback error_callback) override;
  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothTestWin;

  BluetoothAdapterWinrt();
  ~BluetoothAdapterWinrt() override;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_
