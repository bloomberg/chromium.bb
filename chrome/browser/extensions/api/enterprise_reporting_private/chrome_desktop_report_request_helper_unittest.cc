// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace extensions {

class ChromeDesktopReportRequestGeneratorTest : public ::testing::Test {
 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  TestingProfile profile_;
};

TEST_F(ChromeDesktopReportRequestGeneratorTest, OSInfo) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::string expected_os_info;

  // Platform and its version will be the |os_info| by default.
  expected_os_info = base::StringPrintf("{\"os\":\"%s\",\"os_version\":\"%s\"}",
                                        policy::GetOSPlatform().c_str(),
                                        policy::GetOSVersion().c_str());
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_os_info, request->os_info());

  // Platform and its version will be merged with other |os_info|.
  expected_os_info = base::StringPrintf(
      "{\"os\":\"%s\",\"os_version\":\"%s\",\"other_info\":\"info\"}",
      policy::GetOSPlatform().c_str(), policy::GetOSVersion().c_str());
  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::Read("{\"osInfo\": {\"other_info\":\"info\"}}"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_os_info, request->os_info());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, MachineName) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::string expected_machine_name;

  // Machine name will be a empty dict by default.
  expected_machine_name = "{}";
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_machine_name, request->machine_name());

  // Machine name will be copied from the |report|.
  expected_machine_name = "{\"computername\":\"name\"}";
  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::Read("{\"machineName\":{\"computername\":\"name\"}}"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_machine_name, request->machine_name());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ProfileName) {
  // Set the profile name to a known value to compare against.
  const std::string test_name("TEST");
  profile_.set_profile_name(test_name);

  // An empty report suffices for this test. The information of interest is
  // sourced from the profile
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);

  // Make sure the user name was set in the proto.
  EXPECT_EQ(test_name,
            request->browser_report().chrome_user_profile_reports(0).name());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ExtensionList) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::unique_ptr<base::DictionaryValue> report;
  std::string expected_extension_list;

  // Extension list will be a empty list by default.
  expected_extension_list = "[]";
  report = base::DictionaryValue::From(base::JSONReader::Read(
      "{\"browserReport\": {\"chromeUserProfileReport\":[{}]}}"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_extension_list, request->browser_report()
                                         .chrome_user_profile_reports(0)
                                         .extension_data());

  // Extension list will be copied from the |report|.
  expected_extension_list = "[{\"id\":\"1\"}]";
  report = base::DictionaryValue::From(base::JSONReader::Read(
      "{\"browserReport\": "
      "{\"chromeUserProfileReport\":[{\"extensionData\":[{\"id\":\"1\"}]}]}}"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_extension_list, request->browser_report()
                                         .chrome_user_profile_reports(0)
                                         .extension_data());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ProfileID) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;

  // |chrome_user_profile_report| will be created with Profile ID by default.
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(profile_.GetPath().AsUTF8Unsafe(),
            request->browser_report().chrome_user_profile_reports(0).id());

  // Profile ID will be merged into the first item of
  // |chrome_user_profile_reports|
  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::Read("{\"browserReport\": "
                             "{\"chromeUserProfileReport\":[{\"extensionData\":"
                             "[{\"id\":\"1\"}]}]}}"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(profile_.GetPath().AsUTF8Unsafe(),
            request->browser_report().chrome_user_profile_reports(0).id());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, InvalidInput) {
  // |request| will not be generated if the type of input is invalid.
  std::unique_ptr<base::DictionaryValue> report;
  report = base::DictionaryValue::From(
      base::JSONReader::Read("{\"osInfo\": \"info\"}"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));

  report = base::DictionaryValue::From(
      base::JSONReader::Read("{\"machineName\": \"info\"}"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));

  report = base::DictionaryValue::From(base::JSONReader::Read(
      "{\"browserReport\": "
      "{\"chromeUserProfileReport\":[{\"extensionData\":{}}]}}"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));
}

}  // namespace extensions
