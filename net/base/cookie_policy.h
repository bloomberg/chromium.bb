// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_POLICY_H_
#define NET_BASE_COOKIE_POLICY_H_

class GURL;

namespace net {

class CookiePolicy {
 public:
  // Determine if the URL's cookies may be read.
  virtual bool CanGetCookies(const GURL& url,
                             const GURL& first_party_for_cookies) = 0;

  // Determine if the URL's cookies may be written.
  virtual bool CanSetCookie(const GURL& url,
                            const GURL& first_party_for_cookies) = 0;

 protected:
  virtual ~CookiePolicy() {}
};

}  // namespace net

#endif // NET_BASE_COOKIE_POLICY_H_
