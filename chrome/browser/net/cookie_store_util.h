// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_
#define CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_store_factory.h"

class Profile;

namespace net {
class CookieMonsterDelegate;
}  // namespace net

namespace chrome_browser_net {

// Factory method for creating a CookieStore delegate, taking a
// |next_cookie_monster_delegate| which may be NULL. This delegate is stateless
// so only one is necessary per profile.
net::CookieMonsterDelegate* CreateCookieDelegate(
    scoped_refptr<net::CookieMonsterDelegate> next_cookie_monster_delegate);

// Factory method for returning a CookieCryptoDelegate if one is appropriate for
// this platform. The object returned is a LazyInstance. Ownership is not
// transferred.
net::CookieCryptoDelegate* GetCookieCryptoDelegate();

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_COOKIE_STORE_UTIL_H_
