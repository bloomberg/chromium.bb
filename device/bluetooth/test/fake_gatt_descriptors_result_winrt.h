// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTORS_RESULT_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTORS_RESULT_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <vector>

#include "base/macros.h"

namespace device {

class FakeGattDescriptorWinrt;

class FakeGattDescriptorsResultWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDescriptorsResult> {
 public:
  explicit FakeGattDescriptorsResultWinrt(
      const std::vector<Microsoft::WRL::ComPtr<FakeGattDescriptorWinrt>>&
          fake_descriptors);
  ~FakeGattDescriptorsResultWinrt() override;

  // IGattDescriptorsResult:
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCommunicationStatus* value) override;
  IFACEMETHODIMP get_ProtocolError(
      ABI::Windows::Foundation::IReference<uint8_t>** value) override;
  IFACEMETHODIMP get_Descriptors(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptor*>** value) override;

 private:
  std::vector<
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDescriptor>>
      descriptors_;

  DISALLOW_COPY_AND_ASSIGN(FakeGattDescriptorsResultWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTORS_RESULT_WINRT_H_
