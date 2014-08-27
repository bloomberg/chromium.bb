// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_account_tracker_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

// static
KeyedService* FakeAccountTrackerService::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeAccountTrackerService* service = new FakeAccountTrackerService();
  service->Initialize(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      profile->GetPrefs(),
      profile->GetRequestContext());
  return service;
}

FakeAccountTrackerService::FakeAccountTrackerService() {}

FakeAccountTrackerService::~FakeAccountTrackerService() {}

void FakeAccountTrackerService::StartFetchingUserInfo(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}
