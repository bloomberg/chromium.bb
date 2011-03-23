// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_COOKIE_NOTIFICATION_DETAILS_H_
#define CHROME_BROWSER_NET_CHROME_COOKIE_NOTIFICATION_DETAILS_H_
#pragma once

#include "net/base/cookie_monster.h"

struct ChromeCookieDetails {
 public:
  ChromeCookieDetails(const net::CookieMonster::CanonicalCookie* cookie_copy,
                      bool is_removed,
                      net::CookieMonster::Delegate::ChangeCause cause)
      : cookie(cookie_copy),
        removed(is_removed),
        cause(cause) {
  }

  const net::CookieMonster::CanonicalCookie* cookie;
  bool removed;
  net::CookieMonster::Delegate::ChangeCause cause;
};

#endif  // CHROME_BROWSER_NET_CHROME_COOKIE_NOTIFICATION_DETAILS_H_
