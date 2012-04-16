// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_prefs_change.h"
#include "chrome/browser/protector/protected_prefs_watcher.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace protector {

BasePrefsChange::BasePrefsChange() {
}

BasePrefsChange::~BasePrefsChange() {
}

bool BasePrefsChange::Init(Profile* profile) {
  if (!BaseSettingChange::Init(profile))
    return false;
  pref_observer_.reset(new PrefSetObserver(profile->GetPrefs(), this));
  return true;
}

void BasePrefsChange::InitWhenDisabled(Profile* profile) {
  // Forcibly set backup to match the actual settings so that no changes are
  // detected on future runs.
  ProtectorServiceFactory::GetForProfile(profile)->GetPrefsWatcher()->
      ForceUpdateBackup();
}

void BasePrefsChange::DismissOnPrefChange(const std::string& pref_name) {
  DCHECK(!pref_observer_->IsObserved(pref_name));
  pref_observer_->AddPref(pref_name);
}

void BasePrefsChange::IgnorePrefChanges() {
  pref_observer_.reset();
}

void BasePrefsChange::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  const std::string* pref_name = content::Details<std::string>(details).ptr();
  DCHECK(pref_name && pref_observer_->IsObserved(*pref_name));
  // Will delete this instance.
  ProtectorServiceFactory::GetForProfile(profile())->DismissChange(this);
}

}  // namespace protector
