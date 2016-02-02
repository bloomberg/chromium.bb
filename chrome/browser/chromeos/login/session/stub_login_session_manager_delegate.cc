// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/stub_login_session_manager_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"

namespace chromeos {

StubLoginSessionManagerDelegate::StubLoginSessionManagerDelegate(
    Profile* profile,
    const std::string& login_user_id)
    : RestoreAfterCrashSessionManagerDelegate(profile, login_user_id) {
}

StubLoginSessionManagerDelegate::~StubLoginSessionManagerDelegate() {
}

void StubLoginSessionManagerDelegate::Start() {
  session_manager_->SetSessionState(session_manager::SESSION_STATE_ACTIVE);

  // For dev machines and stub user emulate as if sync has been initialized.
  SigninManagerFactory::GetForProfile(profile())
      ->SetAuthenticatedAccountInfo(login_user_id(), login_user_id());
  RestoreAfterCrashSessionManagerDelegate::Start();
}

}  // namespace chromeos
