// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

constexpr char kAssistantOnboardingPrefix[] = "googleassistant://onboarding";
constexpr char kAssistantRemindersPrefix[] = "googleassistant://reminders";
constexpr char kAssistantSettingsPrefix[] = "googleassistant://settings";

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

}  // namespace

bool IsDeepLinkUrl(const GURL& url) {
  return IsAssistantOnboardingDeepLink(url) ||
         IsAssistantRemindersDeepLink(url) || IsAssistantSettingsDeepLink(url);
}

bool IsAssistantOnboardingDeepLink(const GURL& url) {
  return base::StartsWith(url.spec(), kAssistantOnboardingPrefix,
                          base::CompareCase::SENSITIVE);
}

bool IsAssistantRemindersDeepLink(const GURL& url) {
  return base::StartsWith(url.spec(), kAssistantRemindersPrefix,
                          base::CompareCase::SENSITIVE);
}

GURL CreateAssistantSettingsDeepLink() {
  return GURL(kAssistantSettingsPrefix);
}

bool IsAssistantSettingsDeepLink(const GURL& url) {
  return base::StartsWith(url.spec(), kAssistantSettingsPrefix,
                          base::CompareCase::SENSITIVE);
}

base::Optional<GURL> GetWebUrl(const GURL& deep_link) {
  if (!IsWebDeepLink(deep_link))
    return base::nullopt;

  if (IsAssistantRemindersDeepLink(deep_link))
    return GURL(kAssistantRemindersWebUrl);

  if (IsAssistantSettingsDeepLink(deep_link))
    return GURL(kAssistantSettingsWebUrl);

  NOTIMPLEMENTED();
  return base::nullopt;
}

bool IsWebDeepLink(const GURL& url) {
  return IsAssistantRemindersDeepLink(url) || IsAssistantSettingsDeepLink(url);
}

bool ParseDeepLinkParams(const GURL& deep_link,
                         std::map<std::string, std::string>& params) {
  if (!IsDeepLinkUrl(deep_link))
    return false;

  // If the deep link does not have a query then there are no key-value pairs to
  // be parsed. We still return true in this case because omission of parameters
  // in a deep link does not cause a parse attempt to fail.
  if (!deep_link.has_query())
    return true;

  // Key-value pairs are '&' delimited and the keys/values are '=' delimited.
  // Example: "googleassistant://onboarding?k1=v1&k2=v2".
  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(deep_link.query(), '=', '&',
                                          &pairs)) {
    return false;
  }

  // Insert the parsed values into the caller provided map of |params|.
  for (std::pair<std::string, std::string>& pair : pairs) {
    if (params.count(pair.first) > 0) {
      params[pair.first] = pair.second;
    }
  }

  return true;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
