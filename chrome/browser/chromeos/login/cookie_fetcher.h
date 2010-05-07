// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_

#include <string>
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/common/net/url_fetcher.h"

namespace chromeos {

// Given a SID/LSID pair, this class will attempt to turn them into a
// full-fledged set of Google AuthN cookies.
//
// A CookieFetcher manages its own lifecycle.  It deletes itself once it's
// done attempting to fetch URLs.
class CookieFetcher : public URLFetcher::Delegate {
 public:
  // This class is a very thin wrapper around posting a task to the UI thread
  // to call LoginUtils::DoBrowserLaunch().  It's here to allow mocking.
  //
  // In normal usage, instances of this class are owned by a CookieFetcher.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void DoLaunch(Profile* profile) = 0;
  };

  // |profile| is the Profile whose cookie jar you want the cookies in.
  explicit CookieFetcher(Profile* profile);

  // |profile| is the Profile whose cookie jar you want the cookies in.
  // Takes ownership of |cl_handler|, |i_handler|, and |launcher|.
  CookieFetcher(Profile* profile,
                AuthResponseHandler* cl_handler,
                AuthResponseHandler* i_handler,
                Delegate* launcher)
      : profile_(profile),
        client_login_handler_(cl_handler),
        issue_handler_(i_handler),
        launcher_(launcher) {
  }

  // Given a newline-delineated SID/LSID pair of Google cookies (like
  // those that come back from ClientLogin), try to use them to fetch
  // a full-fledged set of Google AuthN cookies.  These cookies will wind up
  // stored in the cookie jar associated with |profile_|, if we get them.
  // Either way, we end up by calling launcher_->DoLaunch()
  void AttemptFetch(const std::string& credentials);

  // Overloaded from URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  class DelegateImpl : public Delegate {
   public:
    DelegateImpl() {}
    ~DelegateImpl() {}
    void DoLaunch(Profile* profile);
   private:
    DISALLOW_COPY_AND_ASSIGN(DelegateImpl);
  };

  virtual ~CookieFetcher() {}

  scoped_ptr<URLFetcher> fetcher_;
  Profile* profile_;
  scoped_ptr<AuthResponseHandler> client_login_handler_;
  scoped_ptr<AuthResponseHandler> issue_handler_;
  scoped_ptr<Delegate> launcher_;

  DISALLOW_COPY_AND_ASSIGN(CookieFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_COOKIE_FETCHER_H_
