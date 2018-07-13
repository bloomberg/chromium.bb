// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <map>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

class DeepLinkUnitTest : public AshTestBase {
 protected:
  DeepLinkUnitTest() = default;
  ~DeepLinkUnitTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeepLinkUnitTest);
};

TEST_F(DeepLinkUnitTest, CreateAssistantSettingsDeepLink) {
  ASSERT_EQ(GURL("googleassistant://settings"),
            CreateAssistantSettingsDeepLink());
}

TEST_F(DeepLinkUnitTest, GetDeepLinkType) {
  const std::map<std::string, DeepLinkType> test_cases = {
      // OK: Supported deep links.
      {"googleassistant://explore", DeepLinkType::kExplore},
      {"googleassistant://onboarding", DeepLinkType::kOnboarding},
      {"googleassistant://reminders", DeepLinkType::kReminders},
      {"googleassistant://send-feedback", DeepLinkType::kFeedback},
      {"googleassistant://settings", DeepLinkType::kSettings},

      // OK: Parameterized deep links.
      {"googleassistant://explore?param=true", DeepLinkType::kExplore},
      {"googleassistant://onboarding?param=true", DeepLinkType::kOnboarding},
      {"googleassistant://reminders?param=true", DeepLinkType::kReminders},
      {"googleassistant://send-feedback?param=true", DeepLinkType::kFeedback},
      {"googleassistant://settings?param=true", DeepLinkType::kSettings},

      // UNSUPPORTED: Deep links are case sensitive.
      {"GOOGLEASSISTANT://EXPLORE", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://ONBOARDING", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://REMINDERS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SETTINGS", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Unknown deep links.
      {"googleassistant://", DeepLinkType::kUnsupported},
      {"googleassistant://unsupported", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Non-deep link URLs.
      {std::string(), DeepLinkType::kUnsupported},
      {"https://www.google.com/", DeepLinkType::kUnsupported}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, GetDeepLinkType(GURL(test_case.first)));
}

// TODO(dmblack): Implement.
TEST_F(DeepLinkUnitTest, IsDeepLinkType) {
  const std::map<std::string, DeepLinkType> test_cases = {
      // OK: Supported deep link types.
      {"googleassistant://explore", DeepLinkType::kExplore},
      {"googleassistant://onboarding", DeepLinkType::kOnboarding},
      {"googleassistant://reminders", DeepLinkType::kReminders},
      {"googleassistant://send-feedback", DeepLinkType::kFeedback},
      {"googleassistant://settings", DeepLinkType::kSettings},

      // OK: Parameterized deep link types.
      {"googleassistant://explore?param=true", DeepLinkType::kExplore},
      {"googleassistant://onboarding?param=true", DeepLinkType::kOnboarding},
      {"googleassistant://reminders?param=true", DeepLinkType::kReminders},
      {"googleassistant://send-feedback?param=true", DeepLinkType::kFeedback},
      {"googleassistant://settings?param=true", DeepLinkType::kSettings},

      // UNSUPPORTED: Deep links are case sensitive.
      {"GOOGLEASSISTANT://EXPLORE", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://ONBOARDING", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://REMINDERS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SETTINGS", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Unknown deep links.
      {"googleassistant://", DeepLinkType::kUnsupported},
      {"googleassistant://unsupported", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Non-deep link URLs.
      {std::string(), DeepLinkType::kUnsupported},
      {"https://www.google.com/", DeepLinkType::kUnsupported}};

  for (const auto& test_case : test_cases)
    ASSERT_TRUE(IsDeepLinkType(GURL(test_case.first), test_case.second));
}

TEST_F(DeepLinkUnitTest, IsDeepLinkUrl) {
  const std::map<std::string, bool> test_cases = {
      // OK: Supported deep links.
      {"googleassistant://explore", true},
      {"googleassistant://onboarding", true},
      {"googleassistant://reminders", true},
      {"googleassistant://send-feedback", true},
      {"googleassistant://settings", true},

      // OK: Parameterized deep links.
      {"googleassistant://explore?param=true", true},
      {"googleassistant://onboarding?param=true", true},
      {"googleassistant://reminders?param=true", true},
      {"googleassistant://send-feedback?param=true", true},
      {"googleassistant://settings?param=true", true},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://EXPLORE", false},
      {"GOOGLEASSISTANT://ONBOARDING", false},
      {"GOOGLEASSISTANT://REMINDERS", false},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", false},
      {"GOOGLEASSISTANT://SETTINGS", false},

      // FAIL: Unknown deep links.
      {"googleassistant://", false},
      {"googleassistant://unsupported", false},

      // FAIL: Non-deep link URLs.
      {std::string(), false},
      {"https://www.google.com/", false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsDeepLinkUrl(GURL(test_case.first)));
}

// TODO(dmblack): Assert actual web URLs when available.
TEST_F(DeepLinkUnitTest, GetWebUrl) {
  const std::map<std::string, bool> test_cases = {
      // OK: Supported web deep links.
      {"googleassistant://explore", true},
      {"googleassistant://reminders", true},
      {"googleassistant://settings", true},

      // OK: Parameterized deep links.
      {"googleassistant://explore?param=true", true},
      {"googleassistant://reminders?param=true", true},
      {"googleassistant://settings?param=true", true},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://EXPLORE", false},
      {"GOOGLEASSISTANT://REMINDERS", false},
      {"GOOGLEASSISTANT://SETTINGS", false},

      // FAIL: Non-web deep links.
      {"googleassistant://onboarding", false},
      {"googleassistant://send-feedback", false},

      // FAIL: Non-deep link URLs.
      {std::string(), false},
      {"https://www.google.com/", false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, GetWebUrl(GURL(test_case.first)).has_value());
}

TEST_F(DeepLinkUnitTest, IsWebDeepLink) {
  const std::map<std::string, bool> test_cases = {
      // OK: Supported web deep links.
      {"googleassistant://explore", true},
      {"googleassistant://reminders", true},
      {"googleassistant://settings", true},

      // OK: Parameterized deep links.
      {"googleassistant://explore?param=true", true},
      {"googleassistant://reminders?param=true", true},
      {"googleassistant://settings?param=true", true},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://EXPLORE", false},
      {"GOOGLEASSISTANT://REMINDERS", false},
      {"GOOGLEASSISTANT://SETTINGS", false},

      // FAIL: Non-web deep links.
      {"googleassistant://onboarding", false},
      {"googleassistant://send-feedback", false},

      // FAIL: Non-deep link URLs.
      {std::string(), false},
      {"https://www.google.com/", false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsWebDeepLink(GURL(test_case.first)));
}

TEST_F(DeepLinkUnitTest, IsWebDeepLinkType) {
  const std::map<DeepLinkType, bool> test_cases = {
      // OK: Supported web deep link types.
      {DeepLinkType::kExplore, true},
      {DeepLinkType::kReminders, true},
      {DeepLinkType::kSettings, true},

      // FAIL: Non-web deep link types.
      {DeepLinkType::kFeedback, false},
      {DeepLinkType::kOnboarding, false},

      // FAIL: Unsupported deep link types.
      {DeepLinkType::kUnsupported, false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsWebDeepLinkType(test_case.first));
}

TEST_F(DeepLinkUnitTest, ParseDeepLinkParams) {
  auto AssertParamsParsed = [](const std::string& deep_link,
                               std::map<std::string, std::string>& params) {
    ASSERT_TRUE(ParseDeepLinkParams(GURL(deep_link), params));
  };

  auto AssertParamsNotParsed = [](const std::string& deep_link,
                                  std::map<std::string, std::string>& params) {
    ASSERT_FALSE(ParseDeepLinkParams(GURL(deep_link), params));
  };

  std::map<std::string, std::string> params;

  auto ResetParams = [&params]() {
    params["k1"] = "default";
    params["k2"] = "default";
  };

  ResetParams();

  // OK: All parameters present.
  AssertParamsParsed("googleassistant://onboarding?k1=v1&k2=v2", params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("v2", params["k2"]);

  ResetParams();

  // OK: Some parameters present.
  AssertParamsParsed("googleassistant://onboarding?k1=v1", params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: Unknown parameters present.
  AssertParamsParsed("googleassistant://onboarding?k1=v1&k3=v3", params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: No parameters present.
  AssertParamsParsed("googleassistant://onboarding", params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: Parameters are case sensitive.
  AssertParamsParsed("googleassistant://onboarding?K1=v1", params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Deep links are case sensitive.
  AssertParamsNotParsed("GOOGLEASSISTANT://ONBOARDING?k1=v1", params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Malformed parameters.
  AssertParamsNotParsed("googleassistant://onboarding?k1=", params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Non-deep link URLs.
  AssertParamsNotParsed("https://www.google.com/search?q=query", params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Empty URLs.
  AssertParamsNotParsed(std::string(), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
