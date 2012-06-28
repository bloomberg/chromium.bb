// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_utils.h"

#include <vector>

#include <bluetooth/bluetooth.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

namespace {
static const char* kCommonUuidPostfix = "-0000-1000-8000-00805f9b34fb";
static const char* kCommonUuidPrefix = "0000";
static const int kUuidSize = 36;
}  // namespace

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
        out_address->b[5 - i] = address_bytes[i];
      }
      return true;
    }
  }

  return false;
}

std::string CanonicalUuid(std::string uuid) {
  if (uuid.empty())
    return "";

  if (uuid.size() < 11 && uuid.find("0x") == 0)
    uuid = uuid.substr(2);

  if (!(uuid.size() == 4 || uuid.size() == 8 || uuid.size() == 36))
    return "";

  if (uuid.size() == 4 || uuid.size() == 8) {
    for (size_t i = 0; i < uuid.size(); ++i) {
      if (!IsHexDigit(uuid[i]))
        return "";
    }

    if (uuid.size() == 4)
      return kCommonUuidPrefix + uuid + kCommonUuidPostfix;

    return uuid + kCommonUuidPostfix;
  }

  std::string uuid_result(uuid);
  for (int i = 0; i < kUuidSize; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-')
        return "";
    } else {
      if (!IsHexDigit(uuid[i]))
        return "";
      uuid_result[i] = tolower(uuid[i]);
    }
  }
  return uuid_result;
}

}  // namespace bluetooth_utils
}  // namespace chromeos
