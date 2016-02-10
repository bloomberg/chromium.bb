// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_

#include "base/files/file.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {

// The BluetoothRemoteGattServiceWin class implements BluetoothGattService
// for remote GATT services on Windows 8 and later.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceWin
    : public BluetoothGattService {
 public:
  BluetoothRemoteGattServiceWin(
      BluetoothDeviceWin* device,
      base::FilePath service_path,
      BluetoothUUID service_uuid,
      uint16_t service_attribute_handle,
      bool is_primary,
      BluetoothRemoteGattServiceWin* parent_service,
      scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);
  ~BluetoothRemoteGattServiceWin() override;

  // Override BluetoothGattService interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothGattCharacteristic*> GetCharacteristics() const override;
  std::vector<BluetoothGattService*> GetIncludedServices() const override;
  BluetoothGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;
  bool AddCharacteristic(BluetoothGattCharacteristic* characteristic) override;
  bool AddIncludedService(BluetoothGattService* service) override;
  void Register(const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void Unregister(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;

  // Update included services and characteristics.
  void Update();
  uint16_t GetAttributeHandle();

 private:
  BluetoothDeviceWin* device_;
  base::FilePath service_path_;
  BluetoothUUID service_uuid_;
  uint16_t service_attribute_handle_;
  bool is_primary_;
  BluetoothRemoteGattServiceWin* parent_service_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceWin);
};

}  // namespace device.
#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WIN_H_
