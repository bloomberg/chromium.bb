// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/stub_login_session_manager_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

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
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   login_user_id());

  RestoreAfterCrashSessionManagerDelegate::Start();
}

}  // namespace chromeos
