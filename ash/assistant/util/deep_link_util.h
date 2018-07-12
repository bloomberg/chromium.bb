// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
#define ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "base/optional.h"

class GURL;

namespace ash {
namespace assistant {
namespace util {

// Returns true if the specified |url| is a deep link, false otherwise.
ASH_EXPORT bool IsDeepLinkUrl(const GURL& url);

// Returns true if the specified |url| is an Assistant explore deep link,
// false otherwise.
ASH_EXPORT bool IsAssistantExploreDeepLink(const GURL& url);

// Returns true if the specified |url| is an Assistant onboarding deep link,
// false otherwise.
ASH_EXPORT bool IsAssistantOnboardingDeepLink(const GURL& url);

// Returns true if the specified |url| is an Assistant reminders deep link,
// false otherwise.
ASH_EXPORT bool IsAssistantRemindersDeepLink(const GURL& url);

// Returns a deep link to top level Assistant Settings.
ASH_EXPORT GURL CreateAssistantSettingsDeepLink();

// Returns true if the specified |url| is an Assistant Settings deep link, false
// otherwise.
ASH_EXPORT bool IsAssistantSettingsDeepLink(const GURL& url);

// Returns the web URL for the specified |deep_link|. A return value will only
// be present if |deep_link| is a web deep link as identified by the
// IsWebDeepLink(GURL) API.
ASH_EXPORT base::Optional<GURL> GetWebUrl(const GURL& deep_link);

// Returns true if the specified |url| is a web deep link, false otherwise.
ASH_EXPORT bool IsWebDeepLink(const GURL& url);

// Parses the specified |deep_link| for the values corresponding to the keys in
// |params|. If a value for a key in |params| is found, it is inserted into the
// map. If not, the existing value in the map is untouched, so it is recommended
// to initiate the map with default values prior to utilizing this API. The
// return value is true if the deep link was successfully parse, false
// otherwise.
ASH_EXPORT bool ParseDeepLinkParams(const GURL& deep_link,
                                    std::map<std::string, std::string>& params);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
