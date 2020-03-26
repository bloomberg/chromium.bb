// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/testing/discovery_utils.h"

#include <sstream>

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

discovery::DnsSdTxtRecord CreateValidTxt() {
  discovery::DnsSdTxtRecord txt;
  txt.SetValue(kUniqueIdKey, kTestUniqueId);
  txt.SetValue(kVersionId, kTestVersion);
  txt.SetValue(kCapabilitiesId, kCapabilitiesStringLong);
  txt.SetValue(kStatusId, kStatus);
  txt.SetValue(kFriendlyNameId, kFriendlyName);
  txt.SetValue(kModelNameId, kModelName);
  return txt;
}

void CompareTxtString(const discovery::DnsSdTxtRecord& txt,
                      const std::string& key,
                      const std::string& expected) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  ASSERT_FALSE(value.is_error())
      << "expected value: '" << expected << "' for key: '" << key
      << "'; got error: " << value.error();
  const std::vector<uint8_t>& data = value.value().get();
  std::string parsed_value = std::string(data.begin(), data.end());
  EXPECT_EQ(parsed_value, expected) << "expected value '"
                                    << "' for key: '" << key << "'";
}

void CompareTxtInt(const discovery::DnsSdTxtRecord& txt,
                   const std::string& key,
                   uint8_t expected) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  ASSERT_FALSE(value.is_error())
      << "key: '" << key << "'' expected: '" << expected << "'";
  const std::vector<uint8_t>& data = value.value().get();
  std::string parsed_value = HexEncode(data);
  ASSERT_EQ(data.size(), size_t{1})
      << "expected one byte value for key: '" << key << "' got size: '"
      << data.size() << "' bytes";
  EXPECT_EQ(data[0], expected)
      << "expected :" << std::hex << expected << "for key: '" << key
      << "', got value: '" << parsed_value << "'";
}

}  // namespace cast
}  // namespace openscreen
