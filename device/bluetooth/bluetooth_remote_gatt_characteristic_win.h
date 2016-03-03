// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_

#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothRemoteGattDescriptorWin;
class BluetoothRemoteGattServiceWin;
class BluetoothTaskManagerWin;

// The BluetoothRemoteGattCharacteristicWin class implements
// BluetoothGattCharacteristic for remote GATT services on Windows 8 and later.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicWin
    : public BluetoothGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicWin(
      BluetoothRemoteGattServiceWin* parent_service,
      BTH_LE_GATT_CHARACTERISTIC* characteristic_info,
      scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);
  ~BluetoothRemoteGattCharacteristicWin() override;

  // Override BluetoothGattCharacteristic interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  std::vector<uint8_t>& GetValue() const override;
  BluetoothGattService* GetService() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;
  bool IsNotifying() const override;
  std::vector<BluetoothGattDescriptor*> GetDescriptors() const override;
  BluetoothGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  bool AddDescriptor(BluetoothGattDescriptor* descriptor) override;
  bool UpdateValue(const std::vector<uint8_t>& value) override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

  // Update included descriptors.
  void Update();
  uint16_t GetAttributeHandle() const;
  BluetoothRemoteGattServiceWin* GetWinService() { return parent_service_; }

 private:
  void OnGetIncludedDescriptorsCallback(
      scoped_ptr<BTH_LE_GATT_DESCRIPTOR> descriptors,
      uint16_t num,
      HRESULT hr);
  void UpdateIncludedDescriptors(PBTH_LE_GATT_DESCRIPTOR descriptors,
                                 uint16_t num);

  // Checks if the descriptor with |uuid| and |attribute_handle| has already
  // been discovered as included descriptor.
  bool IsDescriptorDiscovered(BTH_LE_UUID& uuid, uint16_t attribute_handle);

  // Checks if |descriptor| still exists in this characteristic according to
  // newly discovered |num| of |descriptors|.
  static bool DoesDescriptorExist(PBTH_LE_GATT_DESCRIPTOR descriptors,
                                  uint16_t num,
                                  BluetoothRemoteGattDescriptorWin* descriptor);

  void OnReadRemoteCharacteristicValueCallback(
      scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value,
      HRESULT hr);
  void OnWriteRemoteCharacteristicValueCallback(HRESULT hr);
  BluetoothGattService::GattErrorCode HRESULTToGattErrorCode(HRESULT hr);

  BluetoothRemoteGattServiceWin* parent_service_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  // Characteristic info from OS and used to interact with OS.
  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristic_info_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  BluetoothUUID characteristic_uuid_;
  std::vector<uint8_t> characteristic_value_;
  std::string characteristic_identifier_;

  // The key of GattDescriptorMap is the identitfier of
  // BluetoothRemoteGattDescriptorWin instance.
  typedef std::unordered_map<std::string,
                             scoped_ptr<BluetoothRemoteGattDescriptorWin>>
      GattDescriptorMap;
  GattDescriptorMap included_descriptors_;

  // Flag indicates if characteristic added notification of this characteristic
  // has been sent out to avoid duplicate notification.
  bool characteristic_added_notified_;

  // ReadRemoteCharacteristic request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_characteristic_value_callbacks_;

  // WriteRemoteCharacteristic request callbacks.
  std::pair<base::Closure, ErrorCallback> write_characteristic_value_callbacks_;

  bool characteristic_value_read_or_write_in_progress_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicWin> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_
