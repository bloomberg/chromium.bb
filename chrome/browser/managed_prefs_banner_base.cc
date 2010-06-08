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
    : prefs_(prefs),
      relevant_prefs_(relevant_prefs, relevant_prefs + count) {
  for (PrefSet::const_iterator pref(relevant_prefs_.begin());
       pref != relevant_prefs_.end(); ++pref)
    prefs_->AddPrefObserver(pref->c_str(), this);
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
