// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_

#include <string>

#include "chrome/browser/privacy_blacklist/blacklist.h"

// Generate localized responses to replace blacklisted resources.
// Blacklisted resources such as frames and iframes are replaced
// by HTML. Non visual resources such as Javascript and CSS are
// simply be cancelled so there is no blocked response for them.

namespace BlockedResponse {

// Returns the HTML document used as substituted content for blacklisted
// elements.
std::string GetHTML(const Blacklist::Match* match);

// Returns the image (as a string because that is what is expected by callers)
// used as substituted content for blacklisted elements.
std::string GetImage(const Blacklist::Match* match);

}  // namespace BlockedResponse

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_
