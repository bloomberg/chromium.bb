// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_utils.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {
static const char* kCommonUuidPostfix = "-0000-1000-8000-00805f9b34fb";
static const char* kCommonUuidPrefix = "0000";
static const int kUuidSize = 36;
}  // namespace

namespace device {
namespace bluetooth_utils {

std::string CanonicalUuid(std::string uuid) {
  if (uuid.empty())
    return std::string();

  if (uuid.size() < 11 && uuid.find("0x") == 0)
    uuid = uuid.substr(2);

  if (!(uuid.size() == 4 || uuid.size() == 8 || uuid.size() == 36))
    return std::string();

  if (uuid.size() == 4 || uuid.size() == 8) {
    for (size_t i = 0; i < uuid.size(); ++i) {
      if (!IsHexDigit(uuid[i]))
        return std::string();
    }

    if (uuid.size() == 4)
      return kCommonUuidPrefix + uuid + kCommonUuidPostfix;

    return uuid + kCommonUuidPostfix;
  }

  std::string uuid_result(uuid);
  for (int i = 0; i < kUuidSize; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-')
        return std::string();
    } else {
      if (!IsHexDigit(uuid[i]))
        return std::string();
      uuid_result[i] = tolower(uuid[i]);
    }
  }
  return uuid_result;
}

}  // namespace bluetooth_utils
}  // namespace device
