// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"

namespace chromeos {

namespace {

void UnregisterFlow(const std::string& user_id) {
  UserManager::Get()->ResetUserFlow(user_id);
}

} // namespace


UserFlow::UserFlow() : host_(NULL) {}

UserFlow::~UserFlow() {}

DefaultUserFlow::~DefaultUserFlow() {}

bool DefaultUserFlow::CanLockScreen() {
  return true;
}

bool DefaultUserFlow::ShouldShowSettings() {
  return true;
}

bool DefaultUserFlow::ShouldLaunchBrowser() {
  return true;
}

bool DefaultUserFlow::ShouldSkipPostLoginScreens() {
  return false;
}

bool DefaultUserFlow::HandleLoginFailure(const AuthFailure& failure) {
  return false;
}

void DefaultUserFlow::HandleLoginSuccess(const UserContext& context) {}

bool DefaultUserFlow::HandlePasswordChangeDetected() {
  return false;
}

void DefaultUserFlow::HandleOAuthTokenStatusChange(
    User::OAuthTokenStatus status) {
}

void DefaultUserFlow::LaunchExtraSteps(Profile* profile) {
}

ExtendedUserFlow::ExtendedUserFlow(const std::string& user_id)
    : user_id_(user_id) {
}

ExtendedUserFlow::~ExtendedUserFlow() {
}

bool ExtendedUserFlow::ShouldShowSettings() {
  return true;
}

void ExtendedUserFlow::UnregisterFlowSoon() {
  std::string id_copy(user_id());
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&UnregisterFlow,
                 id_copy));
}

}  // namespace chromeos
