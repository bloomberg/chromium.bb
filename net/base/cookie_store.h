// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_BASE_COOKIE_STORE_H_
#define NET_BASE_COOKIE_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "net/base/cookie_options.h"

class GURL;

namespace net {

class CookieMonster;

// An interface for storing and retrieving cookies. Implementations need to
// be thread safe as its methods can be accessed from IO as well as UI threads.
class CookieStore : public base::RefCountedThreadSafe<CookieStore> {
 public:
  // Set a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  virtual bool SetCookie(const GURL& url, const std::string& cookie_line) = 0;
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options) = 0;
  // Sets a single cookie with a specific creation date. To set a cookie with
  // a creation date of Now() use SetCookie() instead (it calls this function
  // internally).
  virtual bool SetCookieWithCreationTime(const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time) = 0;
  virtual bool SetCookieWithCreationTimeWithOptions(
                                         const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time,
                                         const CookieOptions& options) = 0;
  // Set a vector of response cookie values for the same URL.
  virtual void SetCookies(const GURL& url,
                          const std::vector<std::string>& cookies) = 0;
  virtual void SetCookiesWithOptions(const GURL& url,
                                     const std::vector<std::string>& cookies,
                                     const CookieOptions& options) = 0;

  // TODO what if the total size of all the cookies >4k, can we have a header
  // that big or do we need multiple Cookie: headers?
  // Simple interface, get a cookie string "a=b; c=d" for the given URL.
  // It will _not_ return httponly cookies, see CookieOptions.
  virtual std::string GetCookies(const GURL& url) = 0;
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options) = 0;

  virtual CookieMonster* GetCookieMonster() {
    return NULL;
  };

 protected:
  friend class base::RefCountedThreadSafe<CookieStore>;
  virtual ~CookieStore() {}
};

}  // namespace net

#endif  // NET_BASE_COOKIE_STORE_H_
