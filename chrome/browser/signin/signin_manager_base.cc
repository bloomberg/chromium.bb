// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_base.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager_cookie_helper.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

using namespace signin_internals_util;

using content::BrowserThread;

// static
bool SigninManagerBase::IsAllowedUsername(const std::string& username,
                                          const std::string& policy) {
  if (policy.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  string16 pattern = UTF8ToUTF16(policy);
  if (pattern[0] == L'*')
    pattern.insert(pattern.begin(), L'.');

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << pattern << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  string16 username16 = UTF8ToUTF16(username);
  icu::UnicodeString icu_input(username16.data(), username16.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

SigninManagerBase::SigninManagerBase()
    : profile_(NULL),
      weak_pointer_factory_(this) {
}

SigninManagerBase::~SigninManagerBase() {
  DCHECK(!signin_global_error_.get()) <<
      "SigninManagerBase::Initialize called but not "
      "SigninManagerBase::Shutdown";
}

void SigninManagerBase::Initialize(Profile* profile) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  profile_ = profile;
  signin_global_error_.reset(new SigninGlobalError(this, profile));
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      signin_global_error_.get());
  PrefService* local_state = g_browser_process->local_state();
  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&SigninManagerBase::OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(prefs::kSigninAllowed, profile_->GetPrefs(),
      base::Bind(&SigninManagerBase::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));

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

  if ((!user.empty() && !IsAllowedUsername(user)) || !IsSigninAllowed()) {
    // User is signed in, but the username is invalid - the administrator must
    // have changed the policy since the last signin, so sign out the user.
    SignOut();
  }
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

bool SigninManagerBase::IsAllowedUsername(const std::string& username) const {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return true; // In a unit test with no local state - all names are allowed.

  std::string pattern = local_state->GetString(
      prefs::kGoogleServicesUsernamePattern);
  return IsAllowedUsername(username, pattern);
}

bool SigninManagerBase::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

// static
bool SigninManagerBase::IsSigninAllowedOnIOThread(ProfileIOData* io_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return io_data->signin_allowed()->GetValue();
}

const std::string& SigninManagerBase::GetAuthenticatedUsername() const {
  return authenticated_username_;
}

void SigninManagerBase::SetAuthenticatedUsername(const std::string& username) {
  if (!authenticated_username_.empty()) {
    DLOG_IF(ERROR, username != authenticated_username_) <<
        "Tried to change the authenticated username to something different: " <<
        "Current: " << authenticated_username_ << ", New: " << username;
    return;
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

void SigninManagerBase::SignOut() {
  DCHECK(IsInitialized());

  if (authenticated_username_.empty() && !AuthInProgress())
    return;

  GoogleServiceSignoutDetails details(authenticated_username_);
  authenticated_username_.clear();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);

  // Erase (now) stale information from AboutSigninInternals.
  NotifyDiagnosticsObservers(USERNAME, "");
  NotifyDiagnosticsObservers(LSID, "");
  NotifyDiagnosticsObservers(
      signin_internals_util::SID, "");

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSignoutDetails>(&details));
  token_service->ResetCredentialsInMemory();
  token_service->EraseTokensFromDB();
}

bool SigninManagerBase::AuthInProgress() const {
  // SigninManagerBase never kicks off auth processes itself.
  return false;
}

void SigninManagerBase::Shutdown() {
  if (signin_global_error_.get()) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
        signin_global_error_.get());
    signin_global_error_.reset();
  }
}

void SigninManagerBase::OnGoogleServicesUsernamePatternChanged() {
  if (!authenticated_username_.empty() &&
      !IsAllowedUsername(authenticated_username_)) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut();
  }
}

void SigninManagerBase::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed())
    SignOut();
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
