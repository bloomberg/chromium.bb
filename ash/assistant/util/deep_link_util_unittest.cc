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

using namespace assistant::util;

class DeepLinkUnitTest : public AshTestBase {
 protected:
  DeepLinkUnitTest() = default;
  ~DeepLinkUnitTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeepLinkUnitTest);
};

TEST_F(DeepLinkUnitTest, IsDeepLink) {
  auto AssertDeepLinkUrl = [](const GURL& url) {
    ASSERT_TRUE(IsDeepLinkUrl(url));
  };

  auto AssertNotDeepLinkUrl = [](const GURL& url) {
    ASSERT_FALSE(IsDeepLinkUrl(url));
  };

  // OK: Supported deep links.
  AssertDeepLinkUrl(GURL("googleassistant://explore"));
  AssertDeepLinkUrl(GURL("googleassistant://explore?param=true"));
  AssertDeepLinkUrl(GURL("googleassistant://onboarding"));
  AssertDeepLinkUrl(GURL("googleassistant://onboarding?param=true"));
  AssertDeepLinkUrl(GURL("googleassistant://reminders"));
  AssertDeepLinkUrl(GURL("googleassistant://reminders?param=true"));
  AssertDeepLinkUrl(GURL("googleassistant://settings"));
  AssertDeepLinkUrl(GURL("googleassistant://settings?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotDeepLinkUrl(GURL("GOOGLEASSISTANT://EXPLORE"));
  AssertNotDeepLinkUrl(GURL("GOOGLEASSISTANT://ONBOARDING"));
  AssertNotDeepLinkUrl(GURL("GOOGLEASSISTANT://REMINDERS"));
  AssertNotDeepLinkUrl(GURL("GOOGLEASSISTANT://SETTINGS"));

  // FAIL: Unsupported deep links.
  AssertNotDeepLinkUrl(GURL("googleassistant://"));
  AssertNotDeepLinkUrl(GURL("googleassistant://unsupported"));

  // FAIL: Non-deep link URLs.
  AssertNotDeepLinkUrl(GURL());
  AssertNotDeepLinkUrl(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, IsAssistantExploreDeepLink) {
  auto AssertExploreDeepLink = [](const GURL& url) {
    ASSERT_TRUE(IsAssistantExploreDeepLink(url));
  };

  auto AssertNotExploreDeepLink = [](const GURL& url) {
    ASSERT_FALSE(IsAssistantExploreDeepLink(url));
  };

  // OK: Supported Assistant explore deep links.
  AssertExploreDeepLink(GURL("googleassistant://explore"));
  AssertExploreDeepLink(GURL("googleassistant://explore?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotExploreDeepLink(GURL("GOOGLEASSISTANT://EXPLORE"));

  // FAIL: Non-Assistant explore deep links.
  AssertNotExploreDeepLink(GURL("googleassistant://onboarding"));
  AssertNotExploreDeepLink(GURL("googleassistant://onboarding?param=true"));
  AssertNotExploreDeepLink(GURL("googleassistant://reminders"));
  AssertNotExploreDeepLink(GURL("googleassistant://reminders?param=true"));
  AssertNotExploreDeepLink(GURL("googleassistant://settings"));
  AssertNotExploreDeepLink(GURL("googleassistant://settings?param=true"));

  // FAIL: Non-deep link URLs.
  AssertNotExploreDeepLink(GURL());
  AssertNotExploreDeepLink(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, IsAssistantOnboardingDeepLink) {
  auto AssertOnboardingDeepLink = [](const GURL& url) {
    ASSERT_TRUE(IsAssistantOnboardingDeepLink(url));
  };

  auto AssertNotOnboardingDeepLink = [](const GURL& url) {
    ASSERT_FALSE(IsAssistantOnboardingDeepLink(url));
  };

  // OK: Supported Assistant onboarding deep links.
  AssertOnboardingDeepLink(GURL("googleassistant://onboarding"));
  AssertOnboardingDeepLink(GURL("googleassistant://onboarding?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotOnboardingDeepLink(GURL("GOOGLEASSISTANT://ONBOARDING"));

  // FAIL: Non-Assistant onboarding deep links.
  AssertNotOnboardingDeepLink(GURL("googleassistant://explore"));
  AssertNotOnboardingDeepLink(GURL("googleassistant://explore?param=true"));
  AssertNotOnboardingDeepLink(GURL("googleassistant://reminders"));
  AssertNotOnboardingDeepLink(GURL("googleassistant://reminders?param=true"));
  AssertNotOnboardingDeepLink(GURL("googleassistant://settings"));
  AssertNotOnboardingDeepLink(GURL("googleassistant://settings?param=true"));

  // FAIL: Non-deep link URLs.
  AssertNotOnboardingDeepLink(GURL());
  AssertNotOnboardingDeepLink(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, IsAssistantRemindersDeepLink) {
  auto AssertRemindersDeepLink = [](const GURL& url) {
    ASSERT_TRUE(IsAssistantRemindersDeepLink(url));
  };

  auto AssertNotRemindersDeepLink = [](const GURL& url) {
    ASSERT_FALSE(IsAssistantRemindersDeepLink(url));
  };

  // OK: Supported Assistant reminders deep links.
  AssertRemindersDeepLink(GURL("googleassistant://reminders"));
  AssertRemindersDeepLink(GURL("googleassistant://reminders?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotRemindersDeepLink(GURL("GOOGLEASSISTANT://REMINDERS"));

  // FAIL: Non-Assistant reminders deep links.
  AssertNotRemindersDeepLink(GURL("googleassistant://explore"));
  AssertNotRemindersDeepLink(GURL("googleassistant://explore?param=true"));
  AssertNotRemindersDeepLink(GURL("googleassistant://onboarding"));
  AssertNotRemindersDeepLink(GURL("googleassistant://onboarding?param=true"));
  AssertNotRemindersDeepLink(GURL("googleassistant://settings"));
  AssertNotRemindersDeepLink(GURL("googleassistant://settings?param=true"));

  // FAIL: Non-deep link URLs.
  AssertNotRemindersDeepLink(GURL());
  AssertNotRemindersDeepLink(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, CreateAssistantSettingsDeepLink) {
  ASSERT_EQ(GURL("googleassistant://settings"),
            CreateAssistantSettingsDeepLink());
}

TEST_F(DeepLinkUnitTest, IsAssistantSettingsDeepLink) {
  auto AssertSettingsDeepLink = [](const GURL& url) {
    ASSERT_TRUE(IsAssistantSettingsDeepLink(url));
  };

  auto AssertNotSettingsDeepLink = [](const GURL& url) {
    ASSERT_FALSE(IsAssistantSettingsDeepLink(url));
  };

  // OK: Supported Assistant Settings deep links.
  AssertSettingsDeepLink(GURL("googleassistant://settings"));
  AssertSettingsDeepLink(GURL("googleassistant://settings?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotSettingsDeepLink(GURL("GOOGLEASSISTANT://SETTINGS"));

  // FAIL: Non-Assistant Settings deep links.
  AssertNotSettingsDeepLink(GURL("googleassistant://explore"));
  AssertNotSettingsDeepLink(GURL("googleassistant://explore?param=true"));
  AssertNotSettingsDeepLink(GURL("googleassistant://onboarding"));
  AssertNotSettingsDeepLink(GURL("googleassistant://onboarding?param=true"));
  AssertNotSettingsDeepLink(GURL("googleassistant://reminders"));
  AssertNotSettingsDeepLink(GURL("googleassistant://reminders?param=true"));

  // FAIL: Non-deep link URLs.
  AssertNotSettingsDeepLink(GURL());
  AssertNotSettingsDeepLink(GURL("https://www.google.com/"));
}

// TODO(dmblack): Assert actual web URLs when available.
TEST_F(DeepLinkUnitTest, GetWebUrl) {
  auto AssertWebUrlPresent = [](const GURL& url) {
    ASSERT_TRUE(GetWebUrl(url).has_value());
  };

  auto AssertWebUrlAbsent = [](const GURL& url) {
    ASSERT_FALSE(GetWebUrl(url).has_value());
  };

  // OK: Supported web deep links.
  AssertWebUrlPresent(GURL("googleassistant://explore"));
  AssertWebUrlPresent(GURL("googleassistant://explore?param=true"));
  AssertWebUrlPresent(GURL("googleassistant://reminders"));
  AssertWebUrlPresent(GURL("googleassistant://reminders?param=true"));
  AssertWebUrlPresent(GURL("googleassistant://settings"));
  AssertWebUrlPresent(GURL("googleassistant://settings?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertWebUrlAbsent(GURL("GOOGLEASSISTANT://EXPLORE"));
  AssertWebUrlAbsent(GURL("GOOGLEASSISTANT://REMINDERS"));
  AssertWebUrlAbsent(GURL("GOOGLEASSISTANT://SETTINGS"));

  // FAIL: Non-web deep links.
  AssertWebUrlAbsent(GURL("googleassistant://onboarding"));
  AssertWebUrlAbsent(GURL("googleassistant://onboarding?param=true"));

  // FAIL: Non-deep link URLS.
  AssertWebUrlAbsent(GURL());
  AssertWebUrlAbsent(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, IsWebDeepLink) {
  auto AssertWebDeepLink = [](const GURL& url) {
    ASSERT_TRUE(IsWebDeepLink(url));
  };

  auto AssertNotWebDeepLink = [](const GURL& url) {
    ASSERT_FALSE(IsWebDeepLink(url));
  };

  // OK: Supported web deep links.
  AssertWebDeepLink(GURL("googleassistant://explore"));
  AssertWebDeepLink(GURL("googleassistant://explore?param=true"));
  AssertWebDeepLink(GURL("googleassistant://reminders"));
  AssertWebDeepLink(GURL("googleassistant://reminders?param=true"));
  AssertWebDeepLink(GURL("googleassistant://settings"));
  AssertWebDeepLink(GURL("googleassistant://settings?param=true"));

  // FAIL: Deep links are case sensitive.
  AssertNotWebDeepLink(GURL("GOOGLEASSISTANT://EXPLORE"));
  AssertNotWebDeepLink(GURL("GOOGLEASSISTANT://REMINDERS"));
  AssertNotWebDeepLink(GURL("GOOGLEASSISTANT://SETTINGS"));

  // FAIL: Non-web deep links.
  AssertNotWebDeepLink(GURL("googleassistant://onboarding"));
  AssertNotWebDeepLink(GURL("googleassistant://onboarding?param=true"));

  // FAIL: Non-deep link URLs.
  AssertNotWebDeepLink(GURL());
  AssertNotWebDeepLink(GURL("https://www.google.com/"));
}

TEST_F(DeepLinkUnitTest, ParseDeepLinkParams) {
  auto AssertParamsParsed = [](const GURL& deep_link,
                               std::map<std::string, std::string>& params) {
    ASSERT_TRUE(ParseDeepLinkParams(deep_link, params));
  };

  auto AssertParamsNotParsed = [](const GURL& deep_link,
                                  std::map<std::string, std::string>& params) {
    ASSERT_FALSE(ParseDeepLinkParams(deep_link, params));
  };

  std::map<std::string, std::string> params;

  auto ResetParams = [&params]() {
    params["k1"] = "default";
    params["k2"] = "default";
  };

  ResetParams();

  // OK: All parameters present.
  AssertParamsParsed(GURL("googleassistant://onboarding?k1=v1&k2=v2"), params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("v2", params["k2"]);

  ResetParams();

  // OK: Some parameters present.
  AssertParamsParsed(GURL("googleassistant://onboarding?k1=v1"), params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: Unknown parameters present.
  AssertParamsParsed(GURL("googleassistant://onboarding?k1=v1&k3=v3"), params);
  ASSERT_EQ("v1", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: No parameters present.
  AssertParamsParsed(GURL("googleassistant://onboarding"), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // OK: Parameters are case sensitive.
  AssertParamsParsed(GURL("googleassistant://onboarding?K1=v1"), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Deep links are case sensitive.
  AssertParamsNotParsed(GURL("GOOGLEASSISTANT://ONBOARDING?k1=v1"), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Malformed parameters.
  AssertParamsNotParsed(GURL("googleassistant://onboarding?k1="), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Non-deep link URLs.
  AssertParamsNotParsed(GURL("https://www.google.com/search?q=query"), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);

  ResetParams();

  // FAIL: Empty URLs.
  AssertParamsNotParsed(GURL(), params);
  ASSERT_EQ("default", params["k1"]);
  ASSERT_EQ("default", params["k2"]);
}

}  // namespace ash
