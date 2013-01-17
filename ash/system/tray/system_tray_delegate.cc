// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_delegate.h"

#include "ash/system/tray/test_system_tray_delegate.h"

namespace ash {

NetworkIconInfo::NetworkIconInfo()
    : connecting(false),
      connected(false),
      tray_icon_visible(true),
      is_cellular(false) {
}

NetworkIconInfo::~NetworkIconInfo() {
}

BluetoothDeviceInfo::BluetoothDeviceInfo()
    : connected(false) {
}

BluetoothDeviceInfo::~BluetoothDeviceInfo() {
}

DriveOperationStatus::DriveOperationStatus()
    : progress(0.0), type(OPERATION_OTHER), state(OPERATION_NOT_STARTED) {
}

DriveOperationStatus::~DriveOperationStatus() {
}

IMEInfo::IMEInfo()
    : selected(false),
      third_party(false) {
}

IMEInfo::~IMEInfo() {
}

IMEPropertyInfo::IMEPropertyInfo()
    : selected(false) {
}

IMEPropertyInfo::~IMEPropertyInfo() {
}

// TODO(stevenjb/oshima): Remove this once Shell::delegate_ is guaranteed
// to not be NULL and move TestSystemTrayDelegate -> ash/test. crbug.com/159693
// static
SystemTrayDelegate* SystemTrayDelegate::CreateDummyDelegate() {
  return new test::TestSystemTrayDelegate;
}

}  // namespace ash
