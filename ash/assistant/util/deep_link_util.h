// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
#define ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_

#include "base/optional.h"

class GURL;

namespace ash {
namespace assistant {
namespace util {

// Returns true if the specified |url| is a deep link, false otherwise.
bool IsDeepLinkUrl(const GURL& url);

// Returns a deep link to top level Assistant Settings.
GURL CreateAssistantSettingsDeepLink();

// Returns true if the specified |url| is an Assistant Settings deep link, false
// otherwise.
bool IsAssistantSettingsDeepLink(const GURL& url);

// Returns the web URL for the specified |deep_link|. A return value will only
// be present if |deep_link| is a web deep link as identified by the
// IsWebDeepLink(GURL) API.
base::Optional<GURL> GetWebUrl(const GURL& deep_link);

// Returns true if the specified |url| is a web deep link, false otherwise.
bool IsWebDeepLink(const GURL& url);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
