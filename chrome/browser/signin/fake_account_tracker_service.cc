// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_account_tracker_service.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

// static
KeyedService* FakeAccountTrackerService::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeAccountTrackerService* service = new FakeAccountTrackerService();
  service->Initialize(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile));
  return service;
}

FakeAccountTrackerService::FakeAccountTrackerService() {}

FakeAccountTrackerService::~FakeAccountTrackerService() {}

void FakeAccountTrackerService::StartFetchingUserInfo(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}

void FakeAccountTrackerService::FakeUserInfoFetchSuccess(
    const std::string& account_id,
    const std::string& email,
    const std::string& gaia,
    const std::string& hosted_domain) {
  base::DictionaryValue user_info;
  user_info.SetString("id", gaia);
  user_info.SetString("email", email);
  user_info.SetString("hd", hosted_domain);
  SetAccountStateFromUserInfo(account_id, &user_info);
}

void FakeAccountTrackerService::SendRefreshTokenAnnotationRequest(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}
