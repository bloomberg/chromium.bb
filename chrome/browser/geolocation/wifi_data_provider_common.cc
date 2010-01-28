// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/wifi_data_provider_common.h"

#include "base/string_util.h"

string16 MacAddressAsString16(const uint8 mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static const wchar_t* const kMacFormatString =
      L"%02x-%02x-%02x-%02x-%02x-%02x";
  return WideToUTF16(StringPrintf(kMacFormatString,
                                  mac_as_int[0], mac_as_int[1], mac_as_int[2],
                                  mac_as_int[3], mac_as_int[4], mac_as_int[5]));
}
