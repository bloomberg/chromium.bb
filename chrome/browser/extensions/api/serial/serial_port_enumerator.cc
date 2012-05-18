// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"

namespace extensions {

// static
bool SerialPortEnumerator::DoesPortExist(const StringSet& name_set,
                                         const std::string& port_name) {
  return name_set.count(port_name) == 1;
}

}  // namespace extensions
