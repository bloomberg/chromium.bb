// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_
#define CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_

#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_store_factory.h"

class Profile;

namespace net {
class CookieMonsterDelegate;
}  // namespace net

namespace chrome_browser_net {

// Returns true if cookie-like storage systems should enter record mode for
// debugging.
bool IsCookieRecordMode();

// Returns true if command line flags indicate that cookie-like storage systems
// should be forced to be in memory only.
bool ShouldUseInMemoryCookiesAndCache();

// Factory method for creating a CookieStore delegate that sends
// chrome::NOTIFICATION_COOKIE_CHANGED for the given profile. This
// delegate is stateless so only one is necessary per profile.
net::CookieMonsterDelegate* CreateCookieDelegate(Profile* profile);

// Factory method for returning a CookieCryptoDelegate if one is appropriate for
// this platform. The object returned is a LazyInstance. Ownership is not
// transferred.
content::CookieCryptoDelegate* GetCookieCryptoDelegate();

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_
