// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_COOKIE_HELPER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_COOKIE_HELPER_H_

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}

// This class fetches GAIA cookie on IO thread on behalf of SigninManager which
// only lives on the UI thread.
class SigninManagerCookieHelper
    : public base::RefCountedThreadSafe<SigninManagerCookieHelper> {
 public:
  explicit SigninManagerCookieHelper(
      net::URLRequestContextGetter* request_context_getter);

  // Starts the fetching process, which will notify its completion via
  // callback.
  void StartFetchingGaiaCookiesOnUIThread(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

 private:
  friend class base::RefCountedThreadSafe<SigninManagerCookieHelper>;
  ~SigninManagerCookieHelper();

  // Fetch the GAIA cookies. This must be called in the IO thread.
  void FetchGaiaCookiesOnIOThread();

  // Callback for fetching cookies. This must be called in the IO thread.
  void OnGaiaCookiesFetched(const net::CookieList& cookies);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyOnUIThread(const net::CookieList& cookies);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  // This only mutates on the UI thread.
  base::Callback<void(const net::CookieList& cookies)> completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerCookieHelper);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_COOKIE_HELPER_H_
