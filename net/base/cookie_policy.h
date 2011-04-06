// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_POLICY_H_
#define NET_BASE_COOKIE_POLICY_H_
#pragma once

#include <string>

#include "net/base/completion_callback.h"

class GURL;

namespace net {

// Alternative success codes for CookiePolicy::Can{Get,Set}Cookie(s).
enum {
  OK_FOR_SESSION_ONLY = 1,  // The cookie may be set but not persisted.
};

class CookiePolicy {
 public:
  virtual ~CookiePolicy() {}

  // Determines if the URL's cookies may be read.
  //
  // Returns:
  //   OK                   -  if allowed to read cookies
  //   ERR_ACCESS_DENIED    -  if not allowed to read cookies
  virtual int CanGetCookies(const GURL& url,
                            const GURL& first_party_for_cookies) const = 0;

  // Determines if the URL's cookies may be written.
  //
  // Returns:
  //   OK                   -  if allowed to write cookies
  //   OK_FOR_SESSION_ONLY  -  if allowed to write cookies, but forces them to
  //                           be stored as session cookies
  //   ERR_ACCESS_DENIED    -  if not allowed to write cookies
  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie_line) const = 0;
};

}  // namespace net

#endif // NET_BASE_COOKIE_POLICY_H_
