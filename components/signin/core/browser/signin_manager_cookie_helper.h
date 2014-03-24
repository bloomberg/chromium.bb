// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_COOKIE_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_COOKIE_HELPER_H_

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class URLRequestContextGetter;
}

// This class fetches GAIA cookie on IO thread on behalf of SigninManager which
// only lives on the UI thread.
class SigninManagerCookieHelper
    : public base::RefCountedThreadSafe<SigninManagerCookieHelper> {
 public:
  explicit SigninManagerCookieHelper(
      net::URLRequestContextGetter* request_context_getter,
      scoped_refptr<base::MessageLoopProxy> ui_thread,
      scoped_refptr<base::MessageLoopProxy> io_thread);

  // Starts fetching gaia cookies, which will notify its completion via
  // callback.
  void StartFetchingGaiaCookiesOnUIThread(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

  // Starts fetching cookies for the given URL. This must be called on
  // |ui_thread_|.
  void StartFetchingCookiesOnUIThread(
      const GURL& url,
      const base::Callback<void(const net::CookieList& cookies)>& callback);

 private:
  friend class base::RefCountedThreadSafe<SigninManagerCookieHelper>;
  ~SigninManagerCookieHelper();

  // Fetch cookies for the given URL. This must be called on |io_thread_|.
  void FetchCookiesOnIOThread(const GURL& url);

  // Callback for fetching cookies. This must be called on |io_thread_|.
  void OnCookiesFetched(const net::CookieList& cookies);

  // Notifies the completion callback. This must be called on |ui_thread_|.
  void NotifyOnUIThread(const net::CookieList& cookies);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  // This only mutates on |ui_thread_|.
  base::Callback<void(const net::CookieList& cookies)> completion_callback_;

  // The MessageLoopProxy that this class uses as its UI thread.
  scoped_refptr<base::MessageLoopProxy> ui_thread_;
  // The MessageLoopProxy that this class uses as its IO thread.
  scoped_refptr<base::MessageLoopProxy> io_thread_;
  DISALLOW_COPY_AND_ASSIGN(SigninManagerCookieHelper);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_COOKIE_HELPER_H_
