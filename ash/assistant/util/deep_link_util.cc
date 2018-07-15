// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <set>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Supported deep link prefixes.
constexpr char kAssistantExplorePrefix[] = "googleassistant://explore";
constexpr char kAssistantFeedbackPrefix[] = "googleassistant://send-feedback";
constexpr char kAssistantOnboardingPrefix[] = "googleassistant://onboarding";
constexpr char kAssistantRemindersPrefix[] = "googleassistant://reminders";
constexpr char kAssistantSettingsPrefix[] = "googleassistant://settings";

// TODO(dmblack): Maybe don't hard code this URL. Use a finch flag?
constexpr char kAssistantExploreWebUrl[] =
    "https://assistant.google.com/explore?hl=";

// TODO(dmblack): Wire up actual Assistant Reminders URL.
constexpr char kAssistantRemindersWebUrl[] = R"(data:text/html,
  <html>
    <body style="padding:0 32px;">
      <h3>Assistant Reminders</h3>
      <p>Coming Soon! :)</p>
    </body>
  </html>
)";

// TODO(dmblack): Wire up actual Assistant Settings URL.
constexpr char kAssistantSettingsWebUrl[] = R"(data:text/html,
  <html>
    <body style="padding:0 32px;">
      <h3>Assistant Settings</h3>
      <p>Coming Soon! :)</p>
    </body>
  </html>
)";

// Map of supported deep link types to their prefixes.
const std::map<DeepLinkType, std::string> kSupportedDeepLinks = {
    {DeepLinkType::kExplore, kAssistantExplorePrefix},
    {DeepLinkType::kFeedback, kAssistantFeedbackPrefix},
    {DeepLinkType::kOnboarding, kAssistantOnboardingPrefix},
    {DeepLinkType::kReminders, kAssistantRemindersPrefix},
    {DeepLinkType::kSettings, kAssistantSettingsPrefix}};

// Set of deep link types which open web contents in the Assistant UI.
const std::set<DeepLinkType> kWebDeepLinks = {
    DeepLinkType::kExplore, DeepLinkType::kReminders, DeepLinkType::kSettings};

}  // namespace

GURL CreateAssistantSettingsDeepLink() {
  return GURL(kAssistantSettingsPrefix);
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
