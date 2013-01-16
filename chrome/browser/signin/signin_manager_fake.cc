// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_fake.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

FakeSigninManager::FakeSigninManager(Profile* profile) {
  profile_ = profile;
}

FakeSigninManager::~FakeSigninManager() {}

void FakeSigninManager::StartSignIn(const std::string& username,
                                    const std::string& password,
                                    const std::string& login_token,
                                    const std::string& login_captcha) {
  SetAuthenticatedUsername(username);
}

void FakeSigninManager::StartSignInWithCredentials(
    const std::string& session_index,
    const std::string& username,
    const std::string& password) {
  SetAuthenticatedUsername(username);
}

void FakeSigninManager::StartSignInWithOAuth(const std::string& username,
                                             const std::string& password) {
  SetAuthenticatedUsername(username);
}

void FakeSigninManager::SignOut() {
  authenticated_username_.clear();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

// static
ProfileKeyedService* FakeSigninManager::Build(Profile* profile) {
  return new FakeSigninManager(profile);
}
