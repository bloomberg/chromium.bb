// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for GoogleChromeDistribution class.

#include <windows.h>

#include "base/scoped_ptr.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(GOOGLE_CHROME_BUILD)
TEST(GoogleChromeDistTest, TestExtractUninstallMetrics) {
  // A make-believe JSON preferences file.
  std::string pref_string(
      "{ \n"
      "  \"foo\": \"bar\",\n"
      "  \"uninstall_metrics\": { \n"
      "    \"last_launch_time_sec\": \"1235341118\","
      "    \"last_observed_running_time_sec\": \"1235341183\","
      "    \"launch_count\": \"11\","
      "    \"page_load_count\": \"68\","
      "    \"uptime_sec\": \"809\","
      "    \"installation_date2\": \"1235341141\"\n"
      "  },\n"
      "  \"blah\": {\n"
      "    \"this_sentence_is_true\": false\n"
      "  },\n"
      "  \"user_experience_metrics\": { \n"
      "    \"client_id_timestamp\": \"1234567890\","
      "    \"reporting_enabled\": true\n"
      "  }\n"
      "} \n");

  // The URL string we expect to be generated from said make-believe file.
  std::wstring expected_url_string(
      L"&installation_date2=1235341141"
      L"&last_launch_time_sec=1235341118"
      L"&last_observed_running_time_sec=1235341183"
      L"&launch_count=11&page_load_count=68"
      L"&uptime_sec=809");

  JSONStringValueSerializer json_deserializer(pref_string);
  std::string error_message;

  scoped_ptr<Value> root(json_deserializer.Deserialize(NULL, &error_message));
  ASSERT_TRUE(root.get());
  std::wstring uninstall_metrics_string;

  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER));

  EXPECT_TRUE(
      dist->ExtractUninstallMetrics(*static_cast<DictionaryValue*>(root.get()),
                                    &uninstall_metrics_string));
  EXPECT_EQ(expected_url_string, uninstall_metrics_string);
}
#endif  // defined(GOOGLE_CHROME_BUILD)
