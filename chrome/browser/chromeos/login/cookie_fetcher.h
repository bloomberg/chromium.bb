// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"


class Profile;

namespace chromeos {

// Given a SID/LSID pair, this class will attempt to turn them into a
// full-fledged set of Google AuthN cookies.
//
// A CookieFetcher manages its own lifecycle.  It deletes itself once it's
// done attempting to fetch URLs.
class CookieFetcher : public content::URLFetcherDelegate {
 public:
  // |profile| is the Profile whose cookie jar you want the cookies in.
  explicit CookieFetcher(Profile* profile);

  // |profile| is the Profile whose cookie jar you want the cookies in.
  // Takes ownership of |cl_handler|, |i_handler|, and |launcher|.
  CookieFetcher(Profile* profile,
                AuthResponseHandler* cl_handler,
                AuthResponseHandler* i_handler);

  // Given a newline-delineated SID/LSID pair of Google cookies (like
  // those that come back from ClientLogin), try to use them to fetch
  // a full-fledged set of Google AuthN cookies.  These cookies will wind up
  // stored in the cookie jar associated with |profile_|, if we get them.
  // Either way, we end up by calling launcher_->DoLaunch()
  void AttemptFetch(const std::string& credentials);

  // Overloaded from content::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  virtual ~CookieFetcher();

  scoped_ptr<content::URLFetcher> fetcher_;
  Profile* profile_;
  scoped_ptr<AuthResponseHandler> client_login_handler_;
  scoped_ptr<AuthResponseHandler> issue_handler_;

  DISALLOW_COPY_AND_ASSIGN(CookieFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_
