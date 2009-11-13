// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_

#include <string>
#include <set>

#include "chrome/browser/privacy_blacklist/blacklist.h"

namespace chrome {

extern const char kUnblockScheme[];
extern const char kBlockScheme[];

// Generate localized responses to replace blacklisted resources.
// Blacklisted resources such as frames and iframes are replaced
// by HTML. Non visual resources such as Javascript and CSS are
// simply be cancelled so there is no blocked response for them.

class BlockedResponse {
 public:
  BlockedResponse() {}

  // Returns the HTML document used as substituted content for blacklisted
  // elements.
  std::string GetHTML(const std::string& url, const Blacklist::Match* match);

  // Returns the image (as a string because that is what is expected by callers)
  // used as substituted content for blacklisted elements.
  std::string GetImage(const Blacklist::Match* match);

  // Returns HTTP headers for a blocked response replacing the given url.
  std::string GetHeaders(const std::string& url);

  // Gets the original url of a blocked resource from its blocked url.
  // The input must be a chome-unblock://XXX url. If the unblock url is
  // not found, then about:blank is returned.
  std::string GetOriginalURL(const std::string& url);

 private:
  // Returns a chrome-block://XXX link for the given requested URL.
  std::string GetBlockedURL(const std::string& url);

  // Returns a chrome-unblock://XXX link for the given chrome-block://YYY url.
  std::string GetUnblockedURL(const std::string& url);

  std::set<std::string> blocked_;

  DISALLOW_COPY_AND_ASSIGN(BlockedResponse);
};

}

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLOCKED_RESPONSE_H_
