// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "chrome/browser/signin/token_service_unittest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/webdata/token_web_data.h"
#include "chrome/common/chrome_switches.h"
#include "components/webdata/encryptor/encryptor.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "net/url_request/test_url_fetcher_factory.h"

TokenAvailableTracker::TokenAvailableTracker() {}

TokenAvailableTracker::~TokenAvailableTracker() {}

void TokenAvailableTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    content::Details<const TokenService::TokenAvailableDetails> full = details;
    details_ = *full.ptr();
  }
}

TokenFailedTracker::TokenFailedTracker() {}

TokenFailedTracker::~TokenFailedTracker() {}

void TokenFailedTracker::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED) {
    content::Details<const TokenService::TokenRequestFailedDetails> full =
        details;
    details_ = *full.ptr();
  }
}

TokenServiceTestHarness::TokenServiceTestHarness()
    : signin_manager_(NULL), service_(NULL) {
}

TokenServiceTestHarness::~TokenServiceTestHarness() {}

void TokenServiceTestHarness::SetUp() {
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif
  credentials_.sid = "sid";
  credentials_.lsid = "lsid";
  credentials_.token = "token";
  credentials_.data = "data";
  oauth_token_ = "oauth";
  oauth_secret_ = "secret";

  profile_ = CreateProfile().Pass();

  profile_->CreateWebDataService();

  // Force the loading of the WebDataService.
  TokenWebData::FromBrowserContext(profile_.get());
  base::RunLoop().RunUntilIdle();

  service_ = TokenServiceFactory::GetForProfile(profile_.get());

  success_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_AVAILABLE,
                             content::Source<TokenService>(service_));
  failure_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                             content::Source<TokenService>(service_));

  service()->Initialize("test", profile_.get());
}

scoped_ptr<TestingProfile> TokenServiceTestHarness::CreateProfile() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      ProfileOAuth2TokenServiceFactory::GetInstance(),
      FakeProfileOAuth2TokenService::Build);
  return builder.Build();
}

void TokenServiceTestHarness::CreateSigninManager(const std::string& username) {
  signin_manager_ =
      static_cast<FakeSigninManagerBase*>(
          SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(), FakeSigninManagerBase::Build));
  signin_manager_->Initialize(profile(), NULL);
  signin_manager_->SetAuthenticatedUsername(username);
}

void TokenServiceTestHarness::TearDown() {
  // You have to destroy the profile before the threads are shut down.
  profile_.reset();
}

void TokenServiceTestHarness::UpdateCredentialsOnService() {
  service()->UpdateCredentials(credentials_);
}
