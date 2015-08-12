// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_proximity_auth_client.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"

ChromeProximityAuthClient::ChromeProximityAuthClient(Profile* profile)
    : profile_(profile) {
}

ChromeProximityAuthClient::~ChromeProximityAuthClient() {
}

std::string ChromeProximityAuthClient::GetAuthenticatedUsername() const {
  const SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);
  // |profile_| has to be a signed-in profile with SigninManager already
  // created. Otherwise, just crash to collect stack.
  DCHECK(signin_manager);
  return signin_manager->GetAuthenticatedUsername();
}

void ChromeProximityAuthClient::FinalizeUnlock(bool success) {
  EasyUnlockService::Get(profile_)->FinalizeUnlock(success);
}
