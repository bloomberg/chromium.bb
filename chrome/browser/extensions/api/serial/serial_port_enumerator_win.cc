// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

#include <windows.h>

namespace extensions {

// static
SerialPortEnumerator::StringSet
SerialPortEnumerator::GenerateValidPatterns() {
  const char* VALID_PATTERNS[] = {
    "COM*",
    "\\\\.\\COM*",
  };

  StringSet valid_patterns;
  for (size_t i = 0; i < arraysize(VALID_PATTERNS); ++i)
    valid_patterns.insert(VALID_PATTERNS[i]);

  return valid_patterns;
}

static bool PassesCommConfigTest(const string16& port_name) {
  COMMCONFIG comm_config;
  DWORD cc_size = sizeof(COMMCONFIG);
  return GetDefaultCommConfig(port_name.c_str(), &comm_config, &cc_size) ||
      cc_size != sizeof(COMMCONFIG);
}

// static
SerialPortEnumerator::StringSet
SerialPortEnumerator::GenerateValidSerialPortNames() {
  StringSet name_set;
  int max_port_number = 16;

  // We ended up not using valid_patterns. TODO(miket): we might want
  // to refactor this interface to make it more platform-independent.
  for (int port_number = 0; port_number < max_port_number; ++port_number) {
    string16 device_string16;
    device_string16 = base::StringPrintf(L"\\\\.\\COM%d", port_number);
    if (PassesCommConfigTest(device_string16)) {
      // Keep looking for new ports as long as we're finding them.
      int new_max_port_number = (port_number + 1) * 2;
      if (new_max_port_number > max_port_number)
        max_port_number = new_max_port_number;

      std::string device_string(WideToASCII(device_string16));
      name_set.insert(device_string);
    }
  }
  return name_set;
}

}  // namespace extensions
