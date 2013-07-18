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
  expected.mutable_system_profile()->mutable_hardware()->set_hardware_class(
      "sample-class");

  EXPECT_EQ(expected.SerializeAsString(), encoded);
}

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsLogBaseTest, Hashes) {
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
    {"Back", "0x0557fa923dcee4d0"},
    {"Forward", "0x67d2f6740a8eaebf"},
    {"NewTab", "0x290eb683f96572f1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    uint64 hash = MetricsLogBase::Hash(cases[i].input);
    std::string hash_hex = base::StringPrintf("0x%016" PRIx64, hash);
    EXPECT_EQ(cases[i].output, hash_hex);
  }
};
