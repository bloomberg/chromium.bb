// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager_base.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/common/signin_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

using namespace signin_internals_util;

SigninManagerBase::SigninManagerBase(SigninClient* client)
    : client_(client), initialized_(false), weak_pointer_factory_(this) {}

SigninManagerBase::~SigninManagerBase() {}

void SigninManagerBase::Initialize(PrefService* local_state) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  initialized_ = true;

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService))
    client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);

  std::string user =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  if (!user.empty())
    SetAuthenticatedUsername(user);
}

bool SigninManagerBase::IsInitialized() const { return initialized_; }

bool SigninManagerBase::IsSigninAllowed() const {
  return client_->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
}

const std::string& SigninManagerBase::GetAuthenticatedUsername() const {
  return authenticated_username_;
}

const std::string& SigninManagerBase::GetAuthenticatedAccountId() const {
  return GetAuthenticatedUsername();
}

void SigninManagerBase::SetAuthenticatedUsername(const std::string& username) {
  if (!authenticated_username_.empty()) {
    DLOG_IF(ERROR, !gaia::AreEmailsSame(username, authenticated_username_))
        << "Tried to change the authenticated username to something different: "
        << "Current: " << authenticated_username_ << ", New: " << username;

#if defined(OS_IOS)
    // Prior to M26, chrome on iOS did not normalize the email before setting
    // it in SigninManager.  If the emails are the same as given by
    // gaia::AreEmailsSame() but not the same as given by std::string::op==(),
    // make sure to set the authenticated name below.
    if (!gaia::AreEmailsSame(username, authenticated_username_) ||
        username == authenticated_username_) {
      return;
    }
#else
    return;
#endif
  }
  std::string pref_username =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  DCHECK(pref_username.empty() || gaia::AreEmailsSame(username, pref_username))
      << "username: " << username << "; pref_username: " << pref_username;
  authenticated_username_ = username;
  client_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, username);
  NotifyDiagnosticsObservers(USERNAME, username);

  // Go ahead and update the last signed in username here as well. Once a
  // user is signed in the two preferences should match. Doing it here as
  // opposed to on signin allows us to catch the upgrade scenario.
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername, username);
}

void SigninManagerBase::clear_authenticated_username() {
  authenticated_username_.clear();
}

bool SigninManagerBase::AuthInProgress() const {
  // SigninManagerBase never kicks off auth processes itself.
  return false;
}

void SigninManagerBase::Shutdown() {}

void SigninManagerBase::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninManagerBase::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninManagerBase::AddSigninDiagnosticsObserver(
    SigninDiagnosticsObserver* observer) {
  signin_diagnostics_observers_.AddObserver(observer);
}

void SigninManagerBase::RemoveSigninDiagnosticsObserver(
    SigninDiagnosticsObserver* observer) {
  signin_diagnostics_observers_.RemoveObserver(observer);
}

void SigninManagerBase::NotifyDiagnosticsObservers(
    const UntimedSigninStatusField& field,
    const std::string& value) {
  FOR_EACH_OBSERVER(SigninDiagnosticsObserver,
                    signin_diagnostics_observers_,
                    NotifySigninValueChanged(field, value));
}

void SigninManagerBase::NotifyDiagnosticsObservers(
    const TimedSigninStatusField& field,
    const std::string& value) {
  FOR_EACH_OBSERVER(SigninDiagnosticsObserver,
                    signin_diagnostics_observers_,
                    NotifySigninValueChanged(field, value));
}
