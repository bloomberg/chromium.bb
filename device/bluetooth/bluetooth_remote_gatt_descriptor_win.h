// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothRemoteGattCharacteristicWin;
class BluetoothTaskManagerWin;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorWin
    : public BluetoothGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorWin(
      BluetoothRemoteGattCharacteristicWin* parent_characteristic,
      BTH_LE_GATT_DESCRIPTOR* descriptor_info,
      scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);
  ~BluetoothRemoteGattDescriptorWin() override;

  // Override BluetoothGattDescriptor interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  std::vector<uint8_t>& GetValue() const override;
  BluetoothGattCharacteristic* GetCharacteristic() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;
  void ReadRemoteDescriptor(const ValueCallback& callback,
                            const ErrorCallback& error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;

  uint16_t GetAttributeHandle() const;

 private:
  BluetoothRemoteGattCharacteristicWin* parent_characteristic_;
  scoped_ptr<BTH_LE_GATT_DESCRIPTOR> descriptor_info_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::FilePath service_path_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;
  BluetoothGattCharacteristic::Permissions descriptor_permissions_;
  BluetoothUUID descriptor_uuid_;
  std::string descriptor_identifier_;
  std::vector<uint8_t> descriptor_value_;

  base::WeakPtrFactory<BluetoothRemoteGattDescriptorWin> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorWin);
};

}  // namespace device.
#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WIN_H_
