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

GoogleAutoLoginHelper::~GoogleAutoLoginHelper() {
  DCHECK(accounts_.empty());
}

void GoogleAutoLoginHelper::LogIn() {
  // This is the code path for non-mirror world.
  uber_token_fetcher_.reset(CreateNewUbertokenFetcher());
  uber_token_fetcher_->StartFetchingToken();
  DCHECK(accounts_.empty());
}

void GoogleAutoLoginHelper::LogIn(const std::string& account_id) {
  if (uber_token_fetcher_.get()) {
    accounts_.push_back(account_id);
  } else {
    uber_token_fetcher_.reset(CreateNewUbertokenFetcher());
    uber_token_fetcher_->StartFetchingToken(account_id);
  }
}

void GoogleAutoLoginHelper::OnUbertokenSuccess(const std::string& uber_token) {
  VLOG(1) << "GoogleAutoLoginHelper::OnUbertokenSuccess";
  gaia_auth_fetcher_.reset(CreateNewGaiaAuthFetcher());
  gaia_auth_fetcher_->StartMergeSession(uber_token);
}

void GoogleAutoLoginHelper::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken, error: " << error.ToString();
  if (base::MessageLoop::current()) {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

void GoogleAutoLoginHelper::OnMergeSessionSuccess(const std::string& data) {
  DVLOG(1) << "MergeSession successful." << data;
  if (accounts_.empty()) {
    if (base::MessageLoop::current()) {
      base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    } else {
      delete this;
    }
  } else {
    uber_token_fetcher_.reset(CreateNewUbertokenFetcher());
    uber_token_fetcher_->StartFetchingToken(accounts_.front());
    accounts_.pop_front();
  }
}

void GoogleAutoLoginHelper::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession request, error: " << error.ToString();
  // TODO(acleung): Depending on the return error we should probably
  // take different actions instead of just throw our hands up.

  // Clearning pending accounts for now.
  accounts_.clear();

  if (base::MessageLoop::current()) {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

UbertokenFetcher* GoogleAutoLoginHelper::CreateNewUbertokenFetcher() {
  return new UbertokenFetcher(profile_, this);
}

GaiaAuthFetcher* GoogleAutoLoginHelper::CreateNewGaiaAuthFetcher() {
 return new GaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, profile_->GetRequestContext());
}

