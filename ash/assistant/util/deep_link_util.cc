// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <set>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Supported deep link param keys. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kQueryParamKey[] = "q";
constexpr char kRelaunchParamKey[] = "relaunch";

// Supported deep link prefixes. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kAssistantExplorePrefix[] = "googleassistant://explore";
constexpr char kAssistantFeedbackPrefix[] = "googleassistant://send-feedback";
constexpr char kAssistantOnboardingPrefix[] = "googleassistant://onboarding";
constexpr char kAssistantQueryPrefix[] = "googleassistant://send-query";
constexpr char kAssistantRemindersPrefix[] = "googleassistant://reminders";
constexpr char kAssistantScreenshotPrefix[] =
    "googleassistant://take-screenshot";
constexpr char kAssistantSettingsPrefix[] = "googleassistant://settings";
constexpr char kAssistantWhatsOnMyScreenPrefix[] =
    "googleassistant://whats-on-my-screen";

// TODO(dmblack): Maybe don't hard code this URL. Use a finch flag?
constexpr char kAssistantExploreWebUrl[] =
    "https://assistant.google.com/explore?hl=";

// TODO(dmblack): Wire up actual Assistant Reminders URL.
constexpr char kAssistantRemindersWebUrl[] = R"(data:text/html,
  <html>
    <body style="font-family:Google Sans,sans-serif;padding:0 32px;">
      <h3>Google Assistant Reminders</h3>
      <p>Please use your phone to access Reminders.</p>
    </body>
  </html>
)";

// TODO(dmblack): Wire up actual Assistant Settings URL.
constexpr char kAssistantSettingsWebUrl[] = R"(data:text/html,
  <html>
    <body style="font-family:Google Sans,sans-serif;padding:0 32px;">
      <h3>Google Assistant Settings</h3>
      <p>Please use your phone to access Settings.</p>
    </body>
  </html>
)";

// Map of supported deep link types to their prefixes.
const std::map<DeepLinkType, std::string> kSupportedDeepLinks = {
    {DeepLinkType::kExplore, kAssistantExplorePrefix},
    {DeepLinkType::kFeedback, kAssistantFeedbackPrefix},
    {DeepLinkType::kOnboarding, kAssistantOnboardingPrefix},
    {DeepLinkType::kQuery, kAssistantQueryPrefix},
    {DeepLinkType::kReminders, kAssistantRemindersPrefix},
    {DeepLinkType::kScreenshot, kAssistantScreenshotPrefix},
    {DeepLinkType::kSettings, kAssistantSettingsPrefix},
    {DeepLinkType::kWhatsOnMyScreen, kAssistantWhatsOnMyScreenPrefix}};

// Map of supported deep link params to their keys.
const std::map<DeepLinkParam, std::string> kDeepLinkParamKeys = {
    {DeepLinkParam::kQuery, kQueryParamKey},
    {DeepLinkParam::kRelaunch, kRelaunchParamKey}};

// Set of deep link types which open web contents in the Assistant UI.
const std::set<DeepLinkType> kWebDeepLinks = {
    DeepLinkType::kExplore, DeepLinkType::kReminders, DeepLinkType::kSettings};

}  // namespace

GURL CreateAssistantSettingsDeepLink() {
  return GURL(kAssistantSettingsPrefix);
}

GURL CreateWhatsOnMyScreenDeepLink() {
  return GURL(kAssistantWhatsOnMyScreenPrefix);
}

std::map<std::string, std::string> GetDeepLinkParams(const GURL& deep_link) {
  std::map<std::string, std::string> params;

  if (!IsDeepLinkUrl(deep_link))
    return params;

  if (!deep_link.has_query())
    return params;

  // Key-value pairs are '&' delimited and the keys/values are '=' delimited.
  // Example: "googleassistant://onboarding?k1=v1&k2=v2".
  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(deep_link.query(), '=', '&',
                                          &pairs)) {
    return params;
  }

  for (const auto& pair : pairs)
    params[pair.first] = pair.second;

  return params;
};

base::Optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::string& key = kDeepLinkParamKeys.at(param);
  return params.count(key) == 1
             ? base::Optional<std::string>(net::UnescapeURLComponent(
                   params.at(key), net::UnescapeRule::REPLACE_PLUS_WITH_SPACE))
             : base::nullopt;
}

base::Optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);

  if (value == "true")
    return true;

  if (value == "false")
    return false;

  return base::nullopt;
}

DeepLinkType GetDeepLinkType(const GURL& url) {
  for (const auto& supported_deep_link : kSupportedDeepLinks) {
    if (base::StartsWith(url.spec(), supported_deep_link.second,
                         base::CompareCase::SENSITIVE)) {
      return supported_deep_link.first;
    }
  }
  return DeepLinkType::kUnsupported;
}

bool IsDeepLinkType(const GURL& url, DeepLinkType type) {
  return GetDeepLinkType(url) == type;
}

bool IsDeepLinkUrl(const GURL& url) {
  return GetDeepLinkType(url) != DeepLinkType::kUnsupported;
}

base::Optional<GURL> GetWebUrl(const GURL& deep_link) {
  return GetWebUrl(GetDeepLinkType(deep_link));
}

base::Optional<GURL> GetWebUrl(DeepLinkType type) {
  if (!IsWebDeepLinkType(type))
    return base::nullopt;

  switch (type) {
    case DeepLinkType::kExplore:
      return GURL(kAssistantExploreWebUrl + base::i18n::GetConfiguredLocale());
    case DeepLinkType::kReminders:
      return GURL(kAssistantRemindersWebUrl);
    case DeepLinkType::kSettings:
      return GURL(kAssistantSettingsWebUrl);
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kFeedback:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kScreenshot:
    case DeepLinkType::kWhatsOnMyScreen:
      NOTREACHED();
      return base::nullopt;
  }

  return base::nullopt;
}

bool IsWebDeepLink(const GURL& deep_link) {
  return IsWebDeepLinkType(GetDeepLinkType(deep_link));
}

bool IsWebDeepLinkType(DeepLinkType type) {
  return kWebDeepLinks.count(type) != 0;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
