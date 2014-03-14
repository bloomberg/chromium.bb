// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator_win.h"

#include <windows.h>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace device {

// static
scoped_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return scoped_ptr<SerialDeviceEnumerator>(new SerialDeviceEnumeratorWin());
}

SerialDeviceEnumeratorWin::SerialDeviceEnumeratorWin() {}

SerialDeviceEnumeratorWin::~SerialDeviceEnumeratorWin() {}

// TODO(rockot): Query the system for more information than just device paths.
// This may or may not require using a different strategy than scanning the
// registry location below.
void SerialDeviceEnumeratorWin::GetDevices(SerialDeviceInfoList* devices) {
  devices->clear();

  base::win::RegistryValueIterator iter_key(
      HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM\\");
  for (; iter_key.Valid(); ++iter_key) {
    base::string16 value(iter_key.Value());
    linked_ptr<SerialDeviceInfo> info(new SerialDeviceInfo);
    info->path = base::UTF16ToASCII(value);
    devices->push_back(info);
  }
}

}  // namespace device
