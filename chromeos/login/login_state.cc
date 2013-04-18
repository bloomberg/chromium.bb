// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/login_state.h"

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {

namespace {

// When running a Chrome OS build outside of a device (i.e. on a developer's
// workstation) and not running as login-manager, pretend like we're always
// logged in.
bool AlwaysLoggedIn() {
  return !base::chromeos::IsRunningOnChromeOS() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginManager);
}

}  // namespace

static LoginState* g_login_state = NULL;

// static
void LoginState::Initialize() {
  CHECK(!g_login_state);
  g_login_state = new LoginState();
}

// static
void LoginState::Shutdown() {
  CHECK(g_login_state);
  delete g_login_state;
  g_login_state = NULL;
}

// static
LoginState* LoginState::Get() {
  CHECK(g_login_state) << "LoginState::Get() called before Initialize()";
  return g_login_state;
}

// static
bool LoginState::IsInitialized() {
  return g_login_state;
}

void LoginState::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginState::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void LoginState::SetLoggedInState(LoggedInState state,
                                  LoggedInUserType type) {
  if (state == logged_in_state_)
    return;
  VLOG(1) << "LoggedInState: " << state << " UserType: " << type;
  logged_in_state_ = state;
  logged_in_user_type_ = type;
  NotifyObservers();
}

LoginState::LoggedInState LoginState::GetLoggedInState() const {
  if (AlwaysLoggedIn())
    return LOGGED_IN_ACTIVE;
  return logged_in_state_;
}

LoginState::LoggedInUserType LoginState::GetLoggedInUserType() const {
  return logged_in_user_type_;
}

bool LoginState::IsUserLoggedIn() const {
  return GetLoggedInState() == LOGGED_IN_ACTIVE;
}

bool LoginState::IsUserAuthenticated() const {
  LoggedInUserType type = logged_in_user_type_;
  return type == chromeos::LoginState::LOGGED_IN_USER_REGULAR ||
      type == chromeos::LoginState::LOGGED_IN_USER_OWNER ||
      type == chromeos::LoginState::LOGGED_IN_USER_LOCALLY_MANAGED;
}

// Private methods

LoginState::LoginState() : logged_in_state_(LOGGED_IN_OOBE),
                           logged_in_user_type_(LOGGED_IN_USER_NONE) {
}

LoginState::~LoginState() {
}

void LoginState::NotifyObservers() {
  FOR_EACH_OBSERVER(LoginState::Observer, observer_list_,
                    LoggedInStateChanged(GetLoggedInState()));
}

}  // namespace chromeos
