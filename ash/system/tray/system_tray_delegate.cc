// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_delegate.h"

namespace ash {

NetworkIconInfo::NetworkIconInfo()
    : highlight(false),
      tray_icon_visible(true) {
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

}  // namespace ash
