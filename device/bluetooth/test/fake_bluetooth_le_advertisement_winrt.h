// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class FakeBluetoothLEAdvertisementWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisement> {
 public:
  FakeBluetoothLEAdvertisementWinrt();
  FakeBluetoothLEAdvertisementWinrt(
      base::Optional<std::string> local_name,
      base::Optional<uint8_t> flags,
      BluetoothDevice::UUIDList advertised_uuids,
      base::Optional<int8_t> tx_power,
      BluetoothDevice::ServiceDataMap service_data,
      BluetoothDevice::ManufacturerDataMap manufacturer_data);
  ~FakeBluetoothLEAdvertisementWinrt() override;

  // IBluetoothLEAdvertisement:
  IFACEMETHODIMP get_Flags(ABI::Windows::Foundation::IReference<
                           ABI::Windows::Devices::Bluetooth::Advertisement::
                               BluetoothLEAdvertisementFlags>** value) override;
  IFACEMETHODIMP put_Flags(ABI::Windows::Foundation::IReference<
                           ABI::Windows::Devices::Bluetooth::Advertisement::
                               BluetoothLEAdvertisementFlags>* value) override;
  IFACEMETHODIMP get_LocalName(HSTRING* value) override;
  IFACEMETHODIMP put_LocalName(HSTRING value) override;
  IFACEMETHODIMP get_ServiceUuids(
      ABI::Windows::Foundation::Collections::IVector<GUID>** value) override;
  IFACEMETHODIMP get_ManufacturerData(
      ABI::Windows::Foundation::Collections::IVector<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEManufacturerData*>** value) override;
  IFACEMETHODIMP get_DataSections(
      ABI::Windows::Foundation::Collections::IVector<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementDataSection*>** value) override;
  IFACEMETHODIMP GetManufacturerDataByCompanyId(
      uint16_t company_id,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEManufacturerData*>** data_list) override;
  IFACEMETHODIMP GetSectionsByType(
      uint8_t type,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementDataSection*>** section_list) override;

 private:
  base::Optional<std::string> local_name_;
  base::Optional<uint8_t> flags_;
  BluetoothDevice::UUIDList advertised_uuids_;
  base::Optional<int8_t> tx_power_;
  BluetoothDevice::ServiceDataMap service_data_;
  BluetoothDevice::ManufacturerDataMap manufacturer_data_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothLEAdvertisementWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WINRT_H_
