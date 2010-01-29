// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_POLICY_H_
#define NET_BASE_COOKIE_POLICY_H_

#include "base/basictypes.h"

class GURL;

namespace net {

// The CookiePolicy class implements third-party cookie blocking.
class CookiePolicy {
 public:
  virtual ~CookiePolicy() {}

  // Consult the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be read.
  virtual bool CanGetCookies(const GURL& url,
                             const GURL& first_party_for_cookies);

  // Consult the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be set.
  virtual bool CanSetCookie(const GURL& url,
                            const GURL& first_party_for_cookies);

  enum Type {
    ALLOW_ALL_COOKIES = 0,      // Do not perform any cookie blocking.
    BLOCK_THIRD_PARTY_COOKIES,  // Prevent third-party cookies from being set.
    BLOCK_ALL_COOKIES           // Disable cookies.
  };

  static bool ValidType(int32 type) {
    return type >= ALLOW_ALL_COOKIES && type <= BLOCK_ALL_COOKIES;
  }

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

  // Sets the current policy to enforce. This should be called when the user's
  // preferences change.
  void set_type(Type type) { type_ = type; }

  Type type() const {
    return type_;
  }

  CookiePolicy();

 protected:
  Type type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicy);
};

}  // namespace net

#endif // NET_BASE_COOKIE_POLICY_H_
