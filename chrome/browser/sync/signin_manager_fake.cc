// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/signin_manager_fake.h"

FakeSigninManager::FakeSigninManager() {}

FakeSigninManager::~FakeSigninManager() {}

void FakeSigninManager::StartSignIn(const std::string& username,
                                    const std::string& password,
                                    const std::string& login_token,
                                    const std::string& login_captcha) {
  SetAuthenticatedUsername(username);
}
