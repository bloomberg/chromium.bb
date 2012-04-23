// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_utils.h"

#include <vector>

#include <bluetooth/bluetooth.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace chromeos {
namespace bluetooth_utils {

bool str2ba(const std::string& in_address, bdaddr_t* out_address) {
  if (!out_address)
    return false;

  memset(out_address, 0, sizeof(*out_address));

  if (in_address.size() != 17)
    return false;

  std::string numbers_only;
  for (int i = 0; i < 6; ++i) {
    numbers_only += in_address.substr(i * 3, 2);
  }

  std::vector<uint8> address_bytes;
  if (base::HexStringToBytes(numbers_only, &address_bytes)) {
    if (address_bytes.size() == 6) {
      for (int i = 0; i < 6; ++i) {
        out_address->b[i] = address_bytes[i];
      }
      return true;
    }
  }

  return false;
}

}  // namespace bluetooth_utils
}  // namespace chromeos
