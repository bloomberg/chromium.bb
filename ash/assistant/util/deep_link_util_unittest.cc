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

TEST_F(DeepLinkUnitTest, GetDeepLinkParams) {
  std::map<std::string, std::string> params;

  auto ParseDeepLinkParams = [&params](const std::string& url) {
    params = GetDeepLinkParams(GURL(url));
  };

  // OK: Supported deep link w/ parameters.
  ParseDeepLinkParams("googleassistant://onboarding?k1=v1&k2=v2");
  ASSERT_EQ(2, static_cast<int>(params.size()));
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("v2", params["k2"]);

  // OK: Supported deep link w/o parameters.
  ParseDeepLinkParams("googleassistant://onboarding");
  ASSERT_TRUE(params.empty());

  // FAIL: Unsupported deep link.
  ParseDeepLinkParams("googleassistant://unsupported?k1=v1&k2=v2");
  ASSERT_TRUE(params.empty());

  // FAIL: Non-deep link URLs.
  ParseDeepLinkParams("https://www.google.com/search?q=query");
  ASSERT_TRUE(params.empty());

  // FAIL: Empty URLs.
  ParseDeepLinkParams(std::string());
  ASSERT_TRUE(params.empty());
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

// TODO(dmblack): Assert actual web URLs when available.
TEST_F(DeepLinkUnitTest, GetWebUrlByType) {
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
    ASSERT_EQ(test_case.second, GetWebUrl(test_case.first).has_value());
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

}  // namespace util
}  // namespace assistant
}  // namespace ash
