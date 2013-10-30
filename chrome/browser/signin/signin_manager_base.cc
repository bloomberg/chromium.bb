// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_base.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/signin_manager_cookie_helper.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

using namespace signin_internals_util;

using content::BrowserThread;

SigninManagerBase::SigninManagerBase()
    : profile_(NULL),
      weak_pointer_factory_(this) {
}

SigninManagerBase::~SigninManagerBase() {
}

void SigninManagerBase::Initialize(Profile* profile, PrefService* local_state) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  profile_ = profile;

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService))
    profile->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);

  std::string user = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  if (!user.empty())
    SetAuthenticatedUsername(user);

  InitTokenService();
}

void SigninManagerBase::InitTokenService() {
  // TokenService can be null for unit tests.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (token_service)
    token_service->Initialize(GaiaConstants::kChromeSource, profile_);
  // Note: ChromeOS will kick off TokenService::LoadTokensFromDB from
  // OAuthLoginManager once the rest of the Profile is fully initialized.
}

bool SigninManagerBase::IsInitialized() const {
  return profile_ != NULL;
}

bool SigninManagerBase::IsSigninAllowed() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
}

const std::string& SigninManagerBase::GetAuthenticatedUsername() const {
  return authenticated_username_;
}

void SigninManagerBase::SetAuthenticatedUsername(const std::string& username) {
  if (!authenticated_username_.empty()) {
    DLOG_IF(ERROR, !gaia::AreEmailsSame(username, authenticated_username_)) <<
        "Tried to change the authenticated username to something different: " <<
        "Current: " << authenticated_username_ << ", New: " << username;

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
  authenticated_username_ = username;
  // TODO(tim): We could go further in ensuring kGoogleServicesUsername and
  // authenticated_username_ are consistent once established (e.g. remove
  // authenticated_username_ altogether). Bug 107160.

  NotifyDiagnosticsObservers(USERNAME, username);

  // Go ahead and update the last signed in username here as well. Once a
  // user is signed in the two preferences should match. Doing it here as
  // opposed to on signin allows us to catch the upgrade scenario.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername, username);
}

void SigninManagerBase::clear_authenticated_username() {
  authenticated_username_.clear();
}

bool SigninManagerBase::AuthInProgress() const {
  // SigninManagerBase never kicks off auth processes itself.
  return false;
}

void SigninManagerBase::Shutdown() {
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
