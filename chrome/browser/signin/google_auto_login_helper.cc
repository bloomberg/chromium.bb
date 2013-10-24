// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/google_auto_login_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

GoogleAutoLoginHelper::GoogleAutoLoginHelper(Profile* profile)
    : profile_(profile) {}

GoogleAutoLoginHelper::~GoogleAutoLoginHelper() {}

void GoogleAutoLoginHelper::LogIn() {
  uber_token_fetcher_.reset(new UbertokenFetcher(profile_, this));
  uber_token_fetcher_->StartFetchingToken();
}

void GoogleAutoLoginHelper::LogIn(const std::string& account_id) {
  uber_token_fetcher_.reset(new UbertokenFetcher(profile_, this));
  uber_token_fetcher_->StartFetchingToken(account_id);
}

void GoogleAutoLoginHelper::OnUbertokenSuccess(const std::string& uber_token) {
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, profile_->GetRequestContext()));
  gaia_auth_fetcher_->StartMergeSession(uber_token);
}

void GoogleAutoLoginHelper::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken, error: " << error.ToString();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void GoogleAutoLoginHelper::OnMergeSessionSuccess(const std::string& data) {
  DVLOG(1) << "MergeSession successful." << data;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void GoogleAutoLoginHelper::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession request, error: " << error.ToString();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
