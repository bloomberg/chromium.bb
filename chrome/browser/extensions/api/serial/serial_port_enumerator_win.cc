// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"

#include "base/memory/scoped_ptr.h"
#include "base/win/registry.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

#include <windows.h>

namespace extensions {

SerialPortEnumerator::StringSet
SerialPortEnumerator::GenerateValidSerialPortNames() {
  StringSet name_set;

  base::win::RegistryValueIterator iter_key(HKEY_LOCAL_MACHINE,
      L"HARDWARE\\DEVICEMAP\\SERIALCOMM\\");

  for (; iter_key.Valid(); ++iter_key) {
    string16 str(iter_key.Value());
    std::string device_string(WideToASCII(str));
    name_set.insert(device_string);
  }
  return name_set;
}

}  // namespace extensions
