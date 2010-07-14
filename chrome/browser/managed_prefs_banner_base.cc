// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_prefs_banner_base.h"

#include "chrome/browser/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

ManagedPrefsBannerBase::ManagedPrefsBannerBase(PrefService* prefs,
                                               const wchar_t** relevant_prefs,
                                               size_t count)
    : prefs_(prefs) {
  for (size_t i = 0; i < count; ++i) {
    // Ignore prefs that are not registered.
    const wchar_t* pref = relevant_prefs[i];
    if (prefs->FindPreference(pref)) {
      prefs_->AddPrefObserver(pref, this);
      relevant_prefs_.insert(pref);
    }
  }
}

ManagedPrefsBannerBase::~ManagedPrefsBannerBase() {
  for (PrefSet::const_iterator pref(relevant_prefs_.begin());
       pref != relevant_prefs_.end(); ++pref)
    prefs_->RemovePrefObserver(pref->c_str(), this);
}

void ManagedPrefsBannerBase::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    std::wstring* pref = Details<std::wstring>(details).ptr();
    if (pref && relevant_prefs_.count(*pref))
      OnUpdateVisibility();
  }
}

bool ManagedPrefsBannerBase::DetermineVisibility() const {
  for (PrefSet::const_iterator pref_name(relevant_prefs_.begin());
       pref_name != relevant_prefs_.end(); ++pref_name) {
    const PrefService::Preference* pref =
        prefs_->FindPreference(pref_name->c_str());
    if (pref && pref->IsManaged())
      return true;
  }
  return false;
}
