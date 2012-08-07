// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_fake.h"

FakeSigninManager::FakeSigninManager() {}

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
}

// static
ProfileKeyedService* FakeSigninManager::Build(Profile* profile) {
  return new FakeSigninManager();
}
