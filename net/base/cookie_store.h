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
  // Sets a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options) = 0;

  // TODO what if the total size of all the cookies >4k, can we have a header
  // that big or do we need multiple Cookie: headers?
  // Simple interface, gets a cookie string "a=b; c=d" for the given URL.
  // Use options to access httponly cookies.
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options) = 0;

  // Deletes the passed in cookie for the specified URL.
  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name) = 0;

  // Returns the underlying CookieMonster.
  virtual CookieMonster* GetCookieMonster() = 0;


  // --------------------------------------------------------------------------
  // Helpers to make the above interface simpler for some cases.

  // Sets a cookie for the given URL using default options.
  bool SetCookie(const GURL& url, const std::string& cookie_line) {
    return SetCookieWithOptions(url, cookie_line, CookieOptions());
  }

  // Gets cookies for the given URL using default options.
  std::string GetCookies(const GURL& url) {
    return GetCookiesWithOptions(url, CookieOptions());
  }

  // Sets a vector of response cookie values for the same URL.
  void SetCookiesWithOptions(const GURL& url,
                             const std::vector<std::string>& cookie_lines,
                             const CookieOptions& options) {
    for (size_t i = 0; i < cookie_lines.size(); ++i)
      SetCookieWithOptions(url, cookie_lines[i], options);
  }
  void SetCookies(const GURL& url,
                  const std::vector<std::string>& cookie_lines) {
    SetCookiesWithOptions(url, cookie_lines, CookieOptions());
  }

 protected:
  friend class base::RefCountedThreadSafe<CookieStore>;
  virtual ~CookieStore() {}
};

}  // namespace net

#endif  // NET_BASE_COOKIE_STORE_H_
