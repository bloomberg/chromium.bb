// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_WRAPPER_H_

#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

class Profile;

// A wrapper around FakeProfileOAuth2TokenService to be able to use it as a
// BCKS.
class FakeProfileOAuth2TokenServiceWrapper
    : public ProfileOAuth2TokenServiceWrapper {
 public:
  // Helper function to be used with
  // BrowserContextKeyedService::SetTestingFactory().
  static BrowserContextKeyedService* Build(content::BrowserContext* context);

  // Helper function to be used with
  // BrowserContextKeyedService::SetTestingFactory() that creates a
  // FakeProfileOAuth2TokenService object that posts fetch responses on the
  // current message loop.
  static BrowserContextKeyedService* BuildAutoIssuingTokenService(
      content::BrowserContext* context);

  FakeProfileOAuth2TokenServiceWrapper(Profile* profile,
                                       bool auto_issue_tokens);
  virtual ~FakeProfileOAuth2TokenServiceWrapper();

  // ProfileOAuth2TokenServiceWrapper implementation:
  virtual ProfileOAuth2TokenService* GetProfileOAuth2TokenService() OVERRIDE;

 private:
  FakeProfileOAuth2TokenService service_;
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_WRAPPER_H_
