// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_POLICY_H_
#define NET_BASE_COOKIE_POLICY_H_

#include "net/base/completion_callback.h"

class GURL;

namespace net {

class CookiePolicy {
 public:
  // Determines if the URL's cookies may be read.  Returns OK if allowed to
  // read cookies for the given URL.  Returns ERR_IO_PENDING to indicate that
  // the completion callback will be notified (asynchronously and on the
  // current thread) of the final result.  Note: The completion callback must
  // remain valid until notified.
  virtual int CanGetCookies(const GURL& url,
                            const GURL& first_party_for_cookies,
                            CompletionCallback* callback) = 0;

  // Determines if the URL's cookies may be written.  Returns OK if allowed to
  // write a cookie for the given URL.  Returns ERR_IO_PENDING to indicate that
  // the completion callback will be notified (asynchronously and on the
  // current thread) of the final result.  Note: The completion callback must
  // remain valid until notified.
  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie_line,
                           CompletionCallback* callback) = 0;

 protected:
  virtual ~CookiePolicy() {}
};

}  // namespace net

#endif // NET_BASE_COOKIE_POLICY_H_
