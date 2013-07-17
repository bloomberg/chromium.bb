// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_base.h"

#include <string>

#include "base/base64.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MetricsLogBaseTest, EmptyRecord) {
  MetricsLogBase log("totally bogus client ID", 137, "bogus version");
  log.set_hardware_class("sample-class");
  log.CloseLog();

  std::string encoded;
  log.GetEncodedLog(&encoded);

  // A couple of fields are hard to mock, so these will be copied over directly
  // for the expected output.
  metrics::ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));

  metrics::ChromeUserMetricsExtension expected;
  expected.set_client_id(5217101509553811875);  // Hashed bogus client ID
  expected.set_session_id(137);
  expected.mutable_system_profile()->set_build_timestamp(
      parsed.system_profile().build_timestamp());
  expected.mutable_system_profile()->set_app_version("bogus version");
  expected.mutable_system_profile()->set_channel(
      parsed.system_profile().channel());
#if defined(OS_CHROMEOS)
  expected.mutable_system_profile()->mutable_hardware()->set_hardware_class(
      "sample-class");
#endif  // OS_CHROMEOS

  EXPECT_EQ(expected.SerializeAsString(), encoded);
}

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsLogBaseTest, CreateHashes) {
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
    {"Back", "0x0557fa923dcee4d0"},
    {"Forward", "0x67d2f6740a8eaebf"},
    {"NewTab", "0x290eb683f96572f1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    std::string hash_string;
    uint64 hash_numeric;
    MetricsLogBase::CreateHashes(cases[i].input, &hash_string, &hash_numeric);

    // Verify the numeric representation.
    {
      std::string hash_numeric_hex =
          base::StringPrintf("0x%016" PRIx64, hash_numeric);
      EXPECT_EQ(cases[i].output, hash_numeric_hex);
    }

    // Verify the string representation.
    {
      std::string hash_string_decoded;
      ASSERT_TRUE(base::Base64Decode(hash_string, &hash_string_decoded));

      // Convert to hex string
      // We're only checking the first 8 bytes, because that's what
      // the metrics server uses.
      std::string hash_string_hex = "0x";
      ASSERT_GE(hash_string_decoded.size(), 8U);
      for (size_t j = 0; j < 8; j++) {
        base::StringAppendF(&hash_string_hex, "%02x",
                            static_cast<uint8>(hash_string_decoded.data()[j]));
      }
      EXPECT_EQ(cases[i].output, hash_string_hex);
    }
  }
};
