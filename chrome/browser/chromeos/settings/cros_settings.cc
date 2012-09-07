// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/cros_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/system_settings_provider.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

static base::LazyInstance<CrosSettings> g_cros_settings =
    LAZY_INSTANCE_INITIALIZER;

CrosSettings* CrosSettings::Get() {
  // TODO(xiyaun): Use real stuff when underlying libcros is ready.
  return g_cros_settings.Pointer();
}

bool CrosSettings::IsCrosSettings(const std::string& path) {
  return StartsWithASCII(path, kCrosSettingsPrefix, true);
}

void CrosSettings::Set(const std::string& path, const base::Value& in_value) {
  DCHECK(CalledOnValidThread());
  CrosSettingsProvider* provider;
  provider = GetProvider(path);
  if (provider)
    provider->Set(path, in_value);
}

const base::Value* CrosSettings::GetPref(const std::string& path) const {
  DCHECK(CalledOnValidThread());
  CrosSettingsProvider* provider = GetProvider(path);
  if (provider)
    return provider->Get(path);
  NOTREACHED() << path << " preference was not found in the signed settings.";
  return NULL;
}

CrosSettingsProvider::TrustedStatus CrosSettings::PrepareTrustedValues(
    const base::Closure& callback) const {
  DCHECK(CalledOnValidThread());
  for (size_t i = 0; i < providers_.size(); ++i) {
    CrosSettingsProvider::TrustedStatus status =
        providers_[i]->PrepareTrustedValues(callback);
    if (status != CrosSettingsProvider::TRUSTED)
      return status;
  }
  return CrosSettingsProvider::TRUSTED;
}

void CrosSettings::SetBoolean(const std::string& path, bool in_value) {
  DCHECK(CalledOnValidThread());
  base::FundamentalValue value(in_value);
  Set(path, value);
}

void CrosSettings::SetInteger(const std::string& path, int in_value) {
  DCHECK(CalledOnValidThread());
  base::FundamentalValue value(in_value);
  Set(path, value);
}

void CrosSettings::SetDouble(const std::string& path, double in_value) {
  DCHECK(CalledOnValidThread());
  base::FundamentalValue value(in_value);
  Set(path, value);
}

void CrosSettings::SetString(const std::string& path,
                             const std::string& in_value) {
  DCHECK(CalledOnValidThread());
  base::StringValue value(in_value);
  Set(path, value);
}

void CrosSettings::AppendToList(const std::string& path,
                                const base::Value* value) {
  DCHECK(CalledOnValidThread());
  const base::Value* old_value = GetPref(path);
  scoped_ptr<base::Value> new_value(
      old_value ? old_value->DeepCopy() : new base::ListValue());
  static_cast<base::ListValue*>(new_value.get())->Append(value->DeepCopy());
  Set(path, *new_value);
}

void CrosSettings::RemoveFromList(const std::string& path,
                                  const base::Value* value) {
  DCHECK(CalledOnValidThread());
  const base::Value* old_value = GetPref(path);
  scoped_ptr<base::Value> new_value(
      old_value ? old_value->DeepCopy() : new base::ListValue());
  static_cast<base::ListValue*>(new_value.get())->Remove(*value, NULL);
  Set(path, *new_value);
}

bool CrosSettings::GetBoolean(const std::string& path,
                              bool* bool_value) const {
  DCHECK(CalledOnValidThread());
  const base::Value* value = GetPref(path);
  if (value)
    return value->GetAsBoolean(bool_value);
  return false;
}

bool CrosSettings::GetInteger(const std::string& path,
                              int* out_value) const {
  DCHECK(CalledOnValidThread());
  const base::Value* value = GetPref(path);
  if (value)
    return value->GetAsInteger(out_value);
  return false;
}

bool CrosSettings::GetDouble(const std::string& path,
                             double* out_value) const {
  DCHECK(CalledOnValidThread());
  const base::Value* value = GetPref(path);
  if (value)
    return value->GetAsDouble(out_value);
  return false;
}

bool CrosSettings::GetString(const std::string& path,
                             std::string* out_value) const {
  DCHECK(CalledOnValidThread());
  const base::Value* value = GetPref(path);
  if (value)
    return value->GetAsString(out_value);
  return false;
}

bool CrosSettings::GetList(const std::string& path,
                           const base::ListValue** out_value) const {
  DCHECK(CalledOnValidThread());
  const base::Value* value = GetPref(path);
  if (value)
    return value->GetAsList(out_value);
  return false;
}

bool CrosSettings::FindEmailInList(const std::string& path,
                                   const std::string& email) const {
  DCHECK(CalledOnValidThread());
  std::string canonicalized_email(
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)));
  std::string wildcard_email;
  std::string::size_type at_pos = canonicalized_email.find('@');
  if (at_pos != std::string::npos) {
    wildcard_email =
        std::string("*").append(canonicalized_email.substr(at_pos));
  }

  const base::ListValue* list;
  if (!GetList(path, &list))
    return false;
  for (base::ListValue::const_iterator entry(list->begin());
       entry != list->end();
       ++entry) {
    std::string entry_string;
    if (!(*entry)->GetAsString(&entry_string)) {
      NOTREACHED();
      continue;
    }
    std::string canonicalized_entry(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(entry_string)));

    if (canonicalized_entry == canonicalized_email ||
        canonicalized_entry == wildcard_email) {
      return true;
    }
  }
  return false;
}

bool CrosSettings::AddSettingsProvider(CrosSettingsProvider* provider) {
  DCHECK(CalledOnValidThread());
  providers_.push_back(provider);

  // Allow the provider to notify this object when settings have changed.
  // Providers instantiated inside this class will have the same callback
  // passed to their constructor, but doing it here allows for providers
  // to be instantiated outside this class.
  CrosSettingsProvider::NotifyObserversCallback notify_cb(
      base::Bind(&CrosSettings::FireObservers, base::Unretained(this)));
  provider->SetNotifyObserversCallback(notify_cb);
  return true;
}

bool CrosSettings::RemoveSettingsProvider(CrosSettingsProvider* provider) {
  DCHECK(CalledOnValidThread());
  std::vector<CrosSettingsProvider*>::iterator it =
      std::find(providers_.begin(), providers_.end(), provider);
  if (it != providers_.end()) {
    providers_.erase(it);
    return true;
  }
  return false;
}

void CrosSettings::AddSettingsObserver(const char* path,
                                       content::NotificationObserver* obs) {
  DCHECK(path);
  DCHECK(obs);
  DCHECK(CalledOnValidThread());

  if (!GetProvider(std::string(path))) {
    NOTREACHED() << "Trying to add an observer for an unregistered setting: "
                 << path;
    return;
  }

  // Get the settings observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  SettingsObserverMap::iterator observer_iterator =
      settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end()) {
    observer_list = new NotificationObserverList;
    settings_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  content::NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void CrosSettings::RemoveSettingsObserver(const char* path,
                                          content::NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  SettingsObserverMap::iterator observer_iterator =
      settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end())
    return;

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

CrosSettingsProvider* CrosSettings::GetProvider(
    const std::string& path) const {
  for (size_t i = 0; i < providers_.size(); ++i) {
    if (providers_[i]->HandlesSetting(path))
      return providers_[i];
  }
  return NULL;
}

CrosSettings::CrosSettings() {
  CrosSettingsProvider::NotifyObserversCallback notify_cb(
      base::Bind(&CrosSettings::FireObservers,
                 // This is safe since |this| is never deleted.
                 base::Unretained(this)));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStubCrosSettings)) {
    AddSettingsProvider(new StubCrosSettingsProvider(notify_cb));
  } else {
    AddSettingsProvider(
        new DeviceSettingsProvider(notify_cb, DeviceSettingsService::Get()));
  }
  // System settings are not mocked currently.
  AddSettingsProvider(new SystemSettingsProvider(notify_cb));
}

CrosSettings::~CrosSettings() {
  STLDeleteElements(&providers_);
  STLDeleteValues(&settings_observers_);
}

void CrosSettings::FireObservers(const std::string& path) {
  DCHECK(CalledOnValidThread());
  SettingsObserverMap::iterator observer_iterator =
      settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  content::NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED,
                      content::Source<CrosSettings>(this),
                      content::Details<const std::string>(&path));
  }
}

}  // namespace chromeos
