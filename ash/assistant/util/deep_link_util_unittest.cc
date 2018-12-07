// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <map>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

using DeepLinkUnitTest = AshTestBase;

TEST_F(DeepLinkUnitTest, CreateAssistantSettingsDeepLink) {
  ASSERT_EQ(GURL("googleassistant://settings"),
            CreateAssistantSettingsDeepLink());
}

TEST_F(DeepLinkUnitTest, CreateWhatsOnMyScreenDeepLink) {
  ASSERT_EQ(GURL("googleassistant://whats-on-my-screen"),
            CreateWhatsOnMyScreenDeepLink());
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

TEST_F(DeepLinkUnitTest, GetDeepLinkParam) {
  std::map<std::string, std::string> params = {
      {"action", "0"},  {"extra_time_sec", "60"}, {"id", "timer_id_1"},
      {"page", "main"}, {"q", "query"},           {"relaunch", "true"},
  };

  auto AssertDeepLinkParamEq = [&params](
                                   const base::Optional<std::string>& expected,
                                   DeepLinkParam param) {
    ASSERT_EQ(expected, GetDeepLinkParam(params, param));
  };

  // Case: Deep link parameters present.
  AssertDeepLinkParamEq("main", DeepLinkParam::kPage);
  AssertDeepLinkParamEq("query", DeepLinkParam::kQuery);
  AssertDeepLinkParamEq("true", DeepLinkParam::kRelaunch);
  AssertDeepLinkParamEq("0", DeepLinkParam::kAction);
  AssertDeepLinkParamEq("timer_id_1", DeepLinkParam::kId);
  AssertDeepLinkParamEq("60", DeepLinkParam::kExtraTimeSec);

  // Case: Deep link parameter present, URL encoded.
  params["q"] = "multiple+word+query";
  AssertDeepLinkParamEq("multiple word query", DeepLinkParam::kQuery);

  // Case: Deep link parameters absent.
  params.clear();
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kPage);
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kQuery);
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kRelaunch);
}

TEST_F(DeepLinkUnitTest, GetDeepLinkParamAsBool) {
  std::map<std::string, std::string> params;

  auto AssertDeepLinkParamEq = [&params](const base::Optional<bool>& expected,
                                         DeepLinkParam param) {
    ASSERT_EQ(expected, GetDeepLinkParamAsBool(params, param));
  };

  // Case: Deep link parameter present, well formed "true".
  params["relaunch"] = "true";
  AssertDeepLinkParamEq(true, DeepLinkParam::kRelaunch);

  // Case: Deep link parameter present, well formed "false".
  params["relaunch"] = "false";
  AssertDeepLinkParamEq(false, DeepLinkParam::kRelaunch);

  // Case: Deep link parameter present, incorrect case "true".
  params["relaunch"] = "TRUE";
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kRelaunch);

  // Case: Deep link parameter present, incorrect case "false".
  params["relaunch"] = "FALSE";
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kRelaunch);

  // Case: Deep link parameter present, non-bool value.
  params["relaunch"] = "non-bool";
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kRelaunch);

  // Case: Deep link parameter absent.
  params.clear();
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kRelaunch);
}

TEST_F(DeepLinkUnitTest, GetDeepLinkParamAsInt) {
  std::map<std::string, std::string> params;

  auto AssertDeepLinkParamEq = [&params](const base::Optional<int>& expected,
                                         DeepLinkParam param) {
    ASSERT_EQ(expected, GetDeepLinkParamAsInt(params, param));
  };

  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kExtraTimeSec);

  // Case: Deep link parameter present, well formed "60".
  params["extra_time_sec"] = "60";
  AssertDeepLinkParamEq(60, DeepLinkParam::kExtraTimeSec);
  params["extra_time_sec"] = "00";
  AssertDeepLinkParamEq(0, DeepLinkParam::kExtraTimeSec);

  // Case: Deep link paramter present, non-int value.
  params["extra_time_sec"] = "true";
  AssertDeepLinkParamEq(base::nullopt, DeepLinkParam::kExtraTimeSec);
}

TEST_F(DeepLinkUnitTest, GetDeepLinkParamAsTimerAction) {
  std::map<std::string, std::string> params;

  auto AssertDeepLinkParamEq =
      [&params](const base::Optional<TimerAction>& expected) {
        ASSERT_EQ(expected, GetDeepLinkParamAsTimerAction(params));
      };

  AssertDeepLinkParamEq(base::nullopt);

  // Case: Deep link parameter present, well formed.
  params["action"] = "0";
  AssertDeepLinkParamEq(TimerAction::kAddTimeToTimer);
  params["action"] = "1";
  AssertDeepLinkParamEq(TimerAction::kStop);

  // Case: Deep link paramter present, non-int value.
  params["action"] = "true";
  AssertDeepLinkParamEq(base::nullopt);

  // Case: Deep link paramter present, non-TimerAction value.
  params["action"] = "100";
  AssertDeepLinkParamEq(base::nullopt);
}

TEST_F(DeepLinkUnitTest, GetDeepLinkType) {
  const std::map<std::string, DeepLinkType> test_cases = {
      // OK: Supported deep links.
      {"googleassistant://chrome-settings", DeepLinkType::kChromeSettings},
      {"googleassistant://onboarding", DeepLinkType::kOnboarding},
      {"googleassistant://reminders", DeepLinkType::kReminders},
      {"googleassistant://send-feedback", DeepLinkType::kFeedback},
      {"googleassistant://send-query", DeepLinkType::kQuery},
      {"googleassistant://settings", DeepLinkType::kSettings},
      {"googleassistant://take-screenshot", DeepLinkType::kScreenshot},
      {"googleassistant://task-manager", DeepLinkType::kTaskManager},
      {"googleassistant://timer", DeepLinkType::kTimer},
      {"googleassistant://whats-on-my-screen", DeepLinkType::kWhatsOnMyScreen},

      // OK: Parameterized deep links.
      {"googleassistant://chrome-settings?param=true",
       DeepLinkType::kChromeSettings},
      {"googleassistant://onboarding?param=true", DeepLinkType::kOnboarding},
      {"googleassistant://reminders?param=true", DeepLinkType::kReminders},
      {"googleassistant://send-feedback?param=true", DeepLinkType::kFeedback},
      {"googleassistant://send-query?param=true", DeepLinkType::kQuery},
      {"googleassistant://settings?param=true", DeepLinkType::kSettings},
      {"googleassistant://take-screenshot?param=true",
       DeepLinkType::kScreenshot},
      {"googleassistant://task-manager?param=true", DeepLinkType::kTaskManager},
      {"googleassistant://timer?param=true", DeepLinkType::kTimer},
      {"googleassistant://whats-on-my-screen?param=true",
       DeepLinkType::kWhatsOnMyScreen},

      // UNSUPPORTED: Deep links are case sensitive.
      {"GOOGLEASSISTANT://CHROME-SETTINGS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://ONBOARDING", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://REMINDERS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-QUERY", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SETTINGS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://TAKE-SCREENSHOT", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://TASK-MANAGER", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://TIMER", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://WHATS-ON-MY-SCREEN", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Unknown deep links.
      {"googleassistant://", DeepLinkType::kUnsupported},
      {"googleassistant://unsupported", DeepLinkType::kUnsupported},

      // UNSUPPORTED: Non-deep link URLs.
      {std::string(), DeepLinkType::kUnsupported},
      {"https://www.google.com/", DeepLinkType::kUnsupported}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, GetDeepLinkType(GURL(test_case.first)));
}

TEST_F(DeepLinkUnitTest, IsDeepLinkType) {
  const std::map<std::string, DeepLinkType> test_cases = {
      // OK: Supported deep link types.
      {"googleassistant://chrome-settings", DeepLinkType::kChromeSettings},
      {"googleassistant://onboarding", DeepLinkType::kOnboarding},
      {"googleassistant://reminders", DeepLinkType::kReminders},
      {"googleassistant://send-feedback", DeepLinkType::kFeedback},
      {"googleassistant://send-query", DeepLinkType::kQuery},
      {"googleassistant://settings", DeepLinkType::kSettings},
      {"googleassistant://take-screenshot", DeepLinkType::kScreenshot},
      {"googleassistant://task-manager", DeepLinkType::kTaskManager},
      {"googleassistant://timer", DeepLinkType::kTimer},
      {"googleassistant://whats-on-my-screen", DeepLinkType::kWhatsOnMyScreen},

      // OK: Parameterized deep link types.
      {"googleassistant://chrome-settings?param=true",
       DeepLinkType::kChromeSettings},
      {"googleassistant://onboarding?param=true", DeepLinkType::kOnboarding},
      {"googleassistant://reminders?param=true", DeepLinkType::kReminders},
      {"googleassistant://send-feedback?param=true", DeepLinkType::kFeedback},
      {"googleassistant://send-query?param=true", DeepLinkType::kQuery},
      {"googleassistant://settings?param=true", DeepLinkType::kSettings},
      {"googleassistant://take-screenshot?param=true",
       DeepLinkType::kScreenshot},
      {"googleassistant://task-manager?param=true", DeepLinkType::kTaskManager},
      {"googleassistant://timer?param=true", DeepLinkType::kTimer},
      {"googleassistant://whats-on-my-screen?param=true",
       DeepLinkType::kWhatsOnMyScreen},

      // UNSUPPORTED: Deep links are case sensitive.
      {"GOOGLEASSISTANT://CHROME-SETTINGS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://ONBOARDING", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://REMINDERS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SEND-QUERY", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://SETTINGS", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://TASK-MANAGER", DeepLinkType::kUnsupported},
      {"GOOGLEASSISTANT://TIMER", DeepLinkType::kUnsupported},

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
      {"googleassistant://chrome-settings", true},
      {"googleassistant://onboarding", true},
      {"googleassistant://reminders", true},
      {"googleassistant://send-feedback", true},
      {"googleassistant://send-query", true},
      {"googleassistant://settings", true},
      {"googleassistant://take-screenshot", true},
      {"googleassistant://task-manager", true},
      {"googleassistant://timer", true},
      {"googleassistant://whats-on-my-screen", true},

      // OK: Parameterized deep links.
      {"googleassistant://chrome-settings?param=true", true},
      {"googleassistant://onboarding?param=true", true},
      {"googleassistant://reminders?param=true", true},
      {"googleassistant://send-feedback?param=true", true},
      {"googleassistant://send-query?param=true", true},
      {"googleassistant://settings?param=true", true},
      {"googleassistant://take-screenshot?param=true", true},
      {"googleassistant://task-manager?param=true", true},
      {"googleassistant://timer?param=true", true},
      {"googleassistant://whats-on-my-screen?param=true", true},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://CHROME-SETTINGS", false},
      {"GOOGLEASSISTANT://ONBOARDING", false},
      {"GOOGLEASSISTANT://REMINDERS", false},
      {"GOOGLEASSISTANT://SEND-FEEDBACK", false},
      {"GOOGLEASSISTANT://SEND-QUERY", false},
      {"GOOGLEASSISTANT://SETTINGS", false},
      {"GOOGLEASSISTANT://TAKE-SCREENSHOT", false},
      {"GOOGLEASSISTANT://TASK-MANAGER", false},
      {"GOOGLEASSISTANT://TIMER", false},
      {"GOOGLEASSISTANT://WHATS-ON-MY-SCREEN", false},

      // FAIL: Unknown deep links.
      {"googleassistant://", false},
      {"googleassistant://unsupported", false},

      // FAIL: Non-deep link URLs.
      {std::string(), false},
      {"https://www.google.com/", false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsDeepLinkUrl(GURL(test_case.first)));
}

TEST_F(DeepLinkUnitTest, GetAssistantRemindersUrl) {
  const std::map<base::Optional<std::string>, std::string> test_cases = {
      // OK: Absent/empty id.
      {base::nullopt,
       "https://assistant.google.com/reminders/mainview?hl=en-US"},
      {base::Optional<std::string>(std::string()),
       "https://assistant.google.com/reminders/mainview?hl=en-US"},

      // OK: Specified id.
      {base::Optional<std::string>("123456"),
       "https://assistant.google.com/reminders/id/123456?hl=en-US"}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, GetAssistantRemindersUrl(test_case.first));
}

TEST_F(DeepLinkUnitTest, GetChromeSettingsUrl) {
  const std::map<base::Optional<std::string>, std::string> test_cases = {
      // OK: Absent/empty page.
      {base::nullopt, "chrome://settings/"},
      {base::Optional<std::string>(std::string()), "chrome://settings/"},

      // OK: Allowed pages.
      {base::Optional<std::string>("googleAssistant"),
       "chrome://settings/googleAssistant"},
      {base::Optional<std::string>("languages"), "chrome://settings/languages"},

      // FALLBACK: Allowed pages are case sensitive.
      {base::Optional<std::string>("GOOGLEASSISTANT"), "chrome://settings/"},
      {base::Optional<std::string>("LANGUAGES"), "chrome://settings/"},

      // FALLBACK: Any page not explicitly allowed.
      {base::Optional<std::string>("search"), "chrome://settings/"}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, GetChromeSettingsUrl(test_case.first));
}

TEST_F(DeepLinkUnitTest, GetWebUrl) {
  const std::map<std::string, base::Optional<GURL>> test_cases = {
      // OK: Supported web deep links.
      {"googleassistant://reminders",
       GURL("https://assistant.google.com/reminders/mainview?hl=en-US")},
      {"googleassistant://settings",
       GURL("https://assistant.google.com/settings/mainpage?hl=en-US")},

      // OK: Parameterized deep links.
      {"googleassistant://reminders?id=123456",
       GURL("https://assistant.google.com/reminders/id/123456?hl=en-US")},
      {"googleassistant://settings?param=true",
       GURL("https://assistant.google.com/settings/mainpage?hl=en-US")},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://REMINDERS", base::nullopt},
      {"GOOGLEASSISTANT://SETTINGS", base::nullopt},

      // FAIL: Non-web deep links.
      {"googleassistant://chrome-settings", base::nullopt},
      {"googleassistant://onboarding", base::nullopt},
      {"googleassistant://send-feedback", base::nullopt},
      {"googleassistant://send-query", base::nullopt},
      {"googleassistant://take-screenshot", base::nullopt},
      {"googleassistant://task-manager", base::nullopt},
      {"googleassistant://timer", base::nullopt},
      {"googleassistant://whats-on-my-screen", base::nullopt},

      // FAIL: Non-deep link URLs.
      {std::string(), base::nullopt},
      {"https://www.google.com/", base::nullopt}};

  for (const auto& test_case : test_cases) {
    const base::Optional<GURL>& expected = test_case.second;
    const base::Optional<GURL> actual = GetWebUrl(GURL(test_case.first));

    // Assert |has_value| equivalence.
    ASSERT_EQ(expected, actual);

    // Assert |value| equivalence.
    if (expected)
      ASSERT_EQ(expected.value(), actual.value());
  }
}

TEST_F(DeepLinkUnitTest, GetWebUrlByType) {
  using DeepLinkParams = std::map<std::string, std::string>;
  using TestCase = std::pair<DeepLinkType, DeepLinkParams>;

  // Creates a test case with a single parameter.
  auto CreateTestCaseWithParam =
      [](DeepLinkType type,
         base::Optional<std::pair<std::string, std::string>> param =
             base::nullopt) {
        DeepLinkParams params;
        if (param)
          params.insert(param.value());
        return std::make_pair(type, params);
      };

  // Creates a test case with no parameters.
  auto CreateTestCase = [&CreateTestCaseWithParam](DeepLinkType type) {
    return CreateTestCaseWithParam(type);
  };

  const std::map<TestCase, base::Optional<GURL>> test_cases = {
      // OK: Supported web deep link types.
      {CreateTestCase(DeepLinkType::kReminders),
       GURL("https://assistant.google.com/reminders/mainview?hl=en-US")},
      {CreateTestCaseWithParam(DeepLinkType::kReminders,
                               std::make_pair("id", "123456")),
       GURL("https://assistant.google.com/reminders/id/123456?hl=en-US")},
      {CreateTestCase(DeepLinkType::kSettings),
       GURL("https://assistant.google.com/settings/mainpage?hl=en-US")},

      // FAIL: Non-web deep link types.
      {CreateTestCase(DeepLinkType::kChromeSettings), base::nullopt},
      {CreateTestCase(DeepLinkType::kFeedback), base::nullopt},
      {CreateTestCase(DeepLinkType::kOnboarding), base::nullopt},
      {CreateTestCase(DeepLinkType::kQuery), base::nullopt},
      {CreateTestCase(DeepLinkType::kScreenshot), base::nullopt},
      {CreateTestCase(DeepLinkType::kTaskManager), base::nullopt},
      {CreateTestCase(DeepLinkType::kWhatsOnMyScreen), base::nullopt},

      // FAIL: Unsupported deep link types.
      {CreateTestCase(DeepLinkType::kUnsupported), base::nullopt}};

  for (const auto& test_case : test_cases) {
    const base::Optional<GURL>& expected = test_case.second;
    const base::Optional<GURL> actual = GetWebUrl(
        /*type=*/test_case.first.first, /*params=*/test_case.first.second);

    // Assert |has_value| equivalence.
    ASSERT_EQ(expected, actual);

    // Assert |value| equivalence.
    if (expected)
      ASSERT_EQ(expected.value(), actual.value());
  }
}

TEST_F(DeepLinkUnitTest, IsWebDeepLink) {
  const std::map<std::string, bool> test_cases = {
      // OK: Supported web deep links.
      {"googleassistant://reminders", true},
      {"googleassistant://settings", true},

      // OK: Parameterized deep links.
      {"googleassistant://reminders?param=true", true},
      {"googleassistant://settings?param=true", true},

      // FAIL: Deep links are case sensitive.
      {"GOOGLEASSISTANT://REMINDERS", false},
      {"GOOGLEASSISTANT://SETTINGS", false},

      // FAIL: Non-web deep links.
      {"googleassistant://chrome-settings", false},
      {"googleassistant://onboarding", false},
      {"googleassistant://send-feedback", false},
      {"googleassistant://send-query", false},
      {"googleassistant://take-screenshot", false},
      {"googleassistant://task-manager", false},
      {"googleassistant://timer", false},
      {"googleassistant://whats-on-my-screen", false},

      // FAIL: Non-deep link URLs.
      {std::string(), false},
      {"https://www.google.com/", false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsWebDeepLink(GURL(test_case.first)));
}

TEST_F(DeepLinkUnitTest, IsWebDeepLinkType) {
  const std::map<DeepLinkType, bool> test_cases = {
      // OK: Supported web deep link types.
      {DeepLinkType::kReminders, true},
      {DeepLinkType::kSettings, true},

      // FAIL: Non-web deep link types.
      {DeepLinkType::kChromeSettings, false},
      {DeepLinkType::kFeedback, false},
      {DeepLinkType::kOnboarding, false},
      {DeepLinkType::kQuery, false},
      {DeepLinkType::kScreenshot, false},
      {DeepLinkType::kTaskManager, false},
      {DeepLinkType::kWhatsOnMyScreen, false},

      // FAIL: Unsupported deep link types.
      {DeepLinkType::kUnsupported, false}};

  for (const auto& test_case : test_cases)
    ASSERT_EQ(test_case.second, IsWebDeepLinkType(test_case.first));
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
