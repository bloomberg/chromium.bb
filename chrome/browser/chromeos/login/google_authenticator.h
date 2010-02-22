// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_

#include <string>
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/net/url_fetcher.h"

class LoginStatusConsumer;

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.

class GoogleAuthenticator : public Authenticator,
                            public URLFetcher::Delegate {
 public:
  explicit GoogleAuthenticator(LoginStatusConsumer* consumer)
      : Authenticator(consumer),
        fetcher_(NULL) {
  }
  virtual ~GoogleAuthenticator() {}

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.
  // Returns true if the attempt gets sent successfully and false if not.
  bool Authenticate(const std::string& username,
                    const std::string& password);

  // Overridden from URLFetcher::Delegate
  // When the authentication attempt comes back, will call
  // consumer_->OnLoginSuccess(|username|).
  // On any failure, will call consumer_->OnLoginFailure().
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // I need these static methods so I can PostTasks that use methods
  // of sublcasses of LoginStatusConsumer.  I can't seem to make
  // RunnableMethods out of methods belonging to subclasses without referring
  // to the subclasses specifically, and I want to allow mocked out
  // LoginStatusConsumers to be used here as well.
  static void OnLoginSuccess(LoginStatusConsumer* consumer,
                             const std::string& username);
  static void OnLoginFailure(LoginStatusConsumer* consumer);

 private:
  static const char kCookiePersistence[];
  static const char kAccountType[];
  static const char kSource[];
  static const char kClientLoginUrl[];
  static const char kFormat[];

  scoped_ptr<URLFetcher> fetcher_;
  std::string username_;
  DISALLOW_COPY_AND_ASSIGN(GoogleAuthenticator);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
