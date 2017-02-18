// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/preferences_service.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

PreferencesService::PreferencesService(
    prefs::mojom::PreferencesServiceClientPtr client,
    Profile* profile)
    : preferences_change_registrar_(new PrefChangeRegistrar),
      client_(std::move(client)),
      setting_preferences_(false) {
  DCHECK(profile);
  DCHECK(client_.is_bound());
  service_ = profile->GetPrefs();
  preferences_change_registrar_->Init(service_);
}

PreferencesService::~PreferencesService() {}

void PreferencesService::PreferenceChanged(const std::string& preference_name) {
  if (setting_preferences_)
    return;
  const PrefService::Preference* pref =
      service_->FindPreference(preference_name);
  std::unique_ptr<base::DictionaryValue> dictionary =
      base::MakeUnique<base::DictionaryValue>();
  dictionary->SetWithoutPathExpansion(preference_name,
                                      pref->GetValue()->CreateDeepCopy());
  client_->OnPreferencesChanged(std::move(dictionary));
}

void PreferencesService::SetPreferences(
    std::unique_ptr<base::DictionaryValue> preferences) {
  DCHECK(!setting_preferences_);
  // We ignore preference changes caused by us.
  base::AutoReset<bool> setting_preferences(&setting_preferences_, true);
  for (base::DictionaryValue::Iterator it(*preferences); !it.IsAtEnd();
       it.Advance()) {
    if (!preferences_change_registrar_->IsObserved(it.key()))
      continue;
    const PrefService::Preference* pref = service_->FindPreference(it.key());
    if (!pref) {
      DLOG(ERROR) << "Preference " << it.key() << " not found.\n";
      continue;
    }
    if (it.value().Equals(pref->GetValue()))
      continue;
    service_->Set(it.key(), it.value());
  }
}

void PreferencesService::Subscribe(
    const std::vector<std::string>& preferences) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      base::MakeUnique<base::DictionaryValue>();
  for (auto& it : preferences) {
    const PrefService::Preference* pref = service_->FindPreference(it);
    if (!pref) {
      DLOG(ERROR) << "Preference " << it << " not found.\n";
      continue;
    }
    // PreferenceManager lifetime is managed by a mojo::StrongBindingPtr owned
    // by PreferenceConnectionManager. It will outlive
    // |preferences_change_registrar_| which it owns.
    preferences_change_registrar_->Add(
        it, base::Bind(&PreferencesService::PreferenceChanged,
                       base::Unretained(this)));
    dictionary->SetWithoutPathExpansion(it, pref->GetValue()->CreateDeepCopy());
  }

  if (dictionary->empty())
    return;
  client_->OnPreferencesChanged(std::move(dictionary));
}
