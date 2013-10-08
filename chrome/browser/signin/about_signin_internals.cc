// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals.h"

#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "google_apis/gaia/gaia_constants.h"

using base::Time;
using namespace signin_internals_util;

AboutSigninInternals::AboutSigninInternals() : profile_(NULL) {
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

    // Erase SID and LSID, since those are written as service tokens below.
    if (i == signin_internals_util::SID || i == signin_internals_util::LSID)
      pref_service->SetString(pref_path.c_str(), std::string());

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

  // TODO(rogerta): Get status and timestamps for oauth2 tokens.

  NotifyObservers();
}

void AboutSigninInternals::Initialize(Profile* profile) {
  DCHECK(!profile_);
  profile_ = profile;

  RefreshSigninPrefs();

  SigninManagerFactory::GetForProfile(profile)->
      AddSigninDiagnosticsObserver(this);
  // TODO(rogerta): observe OAuth2TokenService.
}

void AboutSigninInternals::Shutdown() {
  SigninManagerFactory::GetForProfile(profile_)->
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
