// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals.h"

#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "google_apis/gaia/gaia_constants.h"

using base::Time;
using namespace signin_internals_util;

AboutSigninInternals::AboutSigninInternals() : profile_(NULL) {
  // Initialize default values for tokens.
  for (size_t i = 0; i < kNumTokenPrefs; ++i) {
    signin_status_.token_info_map.insert(std::pair<std::string, TokenInfo>(
        kTokenPrefsArray[i],
        TokenInfo(std::string(), "Not Loaded", std::string(), 0,
                  kTokenPrefsArray[i])));
  }
}

AboutSigninInternals::~AboutSigninInternals() {
}

void AboutSigninInternals::AddSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.RemoveObserver(observer);
}

void AboutSigninInternals::NotifySigninValueChanged(
    const UntimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - UNTIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.untimed_signin_fields.size());

  signin_status_.untimed_signin_fields[field_index] = value;

  // Also persist these values in the prefs.
  const std::string pref_path = SigninStatusFieldToString(field);
  profile_->GetPrefs()->SetString(pref_path.c_str(), value);

  NotifyObservers();
}

void AboutSigninInternals::NotifySigninValueChanged(
    const TimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - TIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.timed_signin_fields.size());

  Time now = Time::NowFromSystemTime();
  std::string time_as_str = UTF16ToUTF8(base::TimeFormatFriendlyDate(now));
  TimedSigninStatusValue timed_value(value, time_as_str);

  signin_status_.timed_signin_fields[field_index] = timed_value;

  // Also persist these values in the prefs.
  const std::string value_pref = SigninStatusFieldToString(field) + ".value";
  const std::string time_pref = SigninStatusFieldToString(field) + ".time";
  profile_->GetPrefs()->SetString(value_pref.c_str(), value);
  profile_->GetPrefs()->SetString(time_pref.c_str(), time_as_str);

  NotifyObservers();
}

void AboutSigninInternals::RefreshSigninPrefs() {
  // Return if no profile exists. Can occur in unit tests.
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  for (int i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path =
        SigninStatusFieldToString(static_cast<UntimedSigninStatusField>(i));

    signin_status_.untimed_signin_fields[i - UNTIMED_FIELDS_BEGIN] =
        pref_service->GetString(pref_path.c_str());
  }
  for (int i = TIMED_FIELDS_BEGIN ; i < TIMED_FIELDS_END; ++i) {
    const std::string value_pref = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".value";
    const std::string time_pref = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".time";

    TimedSigninStatusValue value(pref_service->GetString(value_pref.c_str()),
                                 pref_service->GetString(time_pref.c_str()));
    signin_status_.timed_signin_fields[i - TIMED_FIELDS_BEGIN] = value;
  }

  // Get status and timestamps for all token services.
  for (size_t i = 0; i < kNumTokenPrefs; i++) {
    const std::string pref = TokenPrefPath(kTokenPrefsArray[i]);
    const std::string value = pref + ".value";
    const std::string status = pref + ".status";
    const std::string time = pref + ".time";
    const std::string time_internal = pref + ".time_internal";

    TokenInfo token_info(pref_service->GetString(value.c_str()),
                         pref_service->GetString(status.c_str()),
                         pref_service->GetString(time.c_str()),
                         pref_service->GetInt64(time_internal.c_str()),
                         kTokenPrefsArray[i]);

    signin_status_.token_info_map[kTokenPrefsArray[i]] = token_info;
  }

  NotifyObservers();
}

void AboutSigninInternals::NotifyTokenReceivedSuccess(
    const std::string& token_name,
    const std::string& token,
    bool update_time) {
  // This should have been initialized already.
  DCHECK(signin_status_.token_info_map.count(token_name));

  const std::string status_success = "Successful";
  signin_status_.token_info_map[token_name].token = token;
  signin_status_.token_info_map[token_name].status = status_success;

  // Also update preferences.
  const std::string value_pref = TokenPrefPath(token_name) + ".value";
  const std::string time_pref = TokenPrefPath(token_name) + ".time";
  const std::string time_internal_pref =
      TokenPrefPath(token_name) + ".time_internal";
  const std::string status_pref = TokenPrefPath(token_name) + ".status";
  profile_->GetPrefs()->SetString(value_pref.c_str(), token);
  profile_->GetPrefs()->SetString(status_pref.c_str(), "Successful");

  // Update timestamp if needed.
  if (update_time) {
    Time now = Time::NowFromSystemTime();
    int64 time_as_int = now.ToInternalValue();
    const std::string time_as_str =
        UTF16ToUTF8(base::TimeFormatFriendlyDate(now));
    signin_status_.token_info_map[token_name].time = time_as_str;
    signin_status_.token_info_map[token_name].time_internal = time_as_int;
    profile_->GetPrefs()->SetString(time_pref.c_str(), time_as_str);
    profile_->GetPrefs()->SetInt64(time_internal_pref.c_str(), time_as_int);
  }

  NotifyObservers();
}


void AboutSigninInternals::NotifyTokenReceivedFailure(
    const std::string& token_name,
    const std::string& error) {
  Time now = Time::NowFromSystemTime();
  int64 time_as_int = now.ToInternalValue();
  const std::string time_as_str =
      UTF16ToUTF8(base::TimeFormatFriendlyDate(now));

  // This should have been initialized already.
  DCHECK(signin_status_.token_info_map.count(token_name));

  signin_status_.token_info_map[token_name].token.clear();
  signin_status_.token_info_map[token_name].status = error;
  signin_status_.token_info_map[token_name].time = time_as_str;
  signin_status_.token_info_map[token_name].time_internal = time_as_int;

  // Also update preferences.
  const std::string value_pref = TokenPrefPath(token_name) + ".value";
  const std::string time_pref = TokenPrefPath(token_name) + ".time";
  const std::string time_internal_pref =
      TokenPrefPath(token_name) + ".time_internal";
  const std::string status_pref = TokenPrefPath(token_name) + ".status";
  profile_->GetPrefs()->SetString(value_pref.c_str(), std::string());
  profile_->GetPrefs()->SetString(time_pref.c_str(), time_as_str);
    profile_->GetPrefs()->SetInt64(time_internal_pref.c_str(), time_as_int);
  profile_->GetPrefs()->SetString(status_pref.c_str(), error);

  NotifyObservers();
}

// While clearing tokens, we don't update the time or status.
void AboutSigninInternals::NotifyClearStoredToken(
    const std::string& token_name) {
  // This should have been initialized already.
  DCHECK(signin_status_.token_info_map.count(token_name));

  signin_status_.token_info_map[token_name].token.clear();

  const std::string value_pref = TokenPrefPath(token_name) + ".value";
  profile_->GetPrefs()->SetString(value_pref.c_str(), std::string());

  NotifyObservers();
}

void AboutSigninInternals::Initialize(Profile* profile) {
  DCHECK(!profile_);
  profile_ = profile;

  RefreshSigninPrefs();

  SigninManagerFactory::GetForProfile(profile)->
      AddSigninDiagnosticsObserver(this);
  TokenServiceFactory::GetForProfile(profile)->
      AddSigninDiagnosticsObserver(this);
}

void AboutSigninInternals::Shutdown() {
  SigninManagerFactory::GetForProfile(profile_)->
      RemoveSigninDiagnosticsObserver(this);
  TokenServiceFactory::GetForProfile(profile_)->
      RemoveSigninDiagnosticsObserver(this);
}

void AboutSigninInternals::NotifyObservers() {
  FOR_EACH_OBSERVER(AboutSigninInternals::Observer,
                    signin_observers_,
                    OnSigninStateChanged(signin_status_.ToValue()));
}

scoped_ptr<DictionaryValue> AboutSigninInternals::GetSigninStatus() {
  return signin_status_.ToValue().Pass();
}

Time AboutSigninInternals::GetTokenTime(
    const std::string& token_name) const {
  TokenInfoMap::const_iterator iter =
      signin_status_.token_info_map.find(token_name);
  if (iter == signin_status_.token_info_map.end())
    return base::Time();
  return base::Time::FromInternalValue(iter->second.time_internal);
}
