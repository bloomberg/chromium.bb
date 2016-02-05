// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_

#include "device/bluetooth/bluetooth_classic_win.h"

namespace device {
namespace win {
// Fake implementation of BluetoothClassicWrapper. Used for BluetoothTestWin.
class BluetoothClassicWrapperFake : public BluetoothClassicWrapper {
 public:
  BluetoothClassicWrapperFake();
  ~BluetoothClassicWrapperFake() override;

  HBLUETOOTH_RADIO_FIND FindFirstRadio(
      const BLUETOOTH_FIND_RADIO_PARAMS* params,
      HANDLE* out_handle) override;
  DWORD GetRadioInfo(HANDLE handle,
                     PBLUETOOTH_RADIO_INFO out_radio_info) override;
  BOOL FindRadioClose(HBLUETOOTH_RADIO_FIND handle) override;
  BOOL IsConnectable(HANDLE handle) override;
  HBLUETOOTH_DEVICE_FIND FindFirstDevice(
      const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
      BLUETOOTH_DEVICE_INFO* out_device_info) override;
  BOOL FindNextDevice(HBLUETOOTH_DEVICE_FIND handle,
                      BLUETOOTH_DEVICE_INFO* out_device_info) override;
  BOOL FindDeviceClose(HBLUETOOTH_DEVICE_FIND handle) override;
  BOOL EnableDiscovery(HANDLE handle, BOOL is_enable) override;
  BOOL EnableIncomingConnections(HANDLE handle, BOOL is_enable) override;
};

}  // namespace device
}  // namespace win

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_
