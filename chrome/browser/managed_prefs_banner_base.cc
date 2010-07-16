// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_prefs_banner_base.h"

#include "chrome/browser/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

ManagedPrefsBannerBase::ManagedPrefsBannerBase(PrefService* prefs,
                                               OptionsPage page)
    : prefs_(prefs) {
  switch (page) {
    case OPTIONS_PAGE_GENERAL:
      AddPref(prefs::kHomePage);
      AddPref(prefs::kHomePageIsNewTabPage);
      break;
    case OPTIONS_PAGE_CONTENT:
      AddPref(prefs::kSyncManaged);
      AddPref(prefs::kPasswordManagerEnabled);
      break;
    case OPTIONS_PAGE_ADVANCED:
      AddPref(prefs::kAlternateErrorPagesEnabled);
      AddPref(prefs::kSearchSuggestEnabled);
      AddPref(prefs::kDnsPrefetchingEnabled);
      AddPref(prefs::kSafeBrowsingEnabled);
#if defined(GOOGLE_CHROME_BUILD)
      AddPref(prefs::kMetricsReportingEnabled);
#endif
      break;
    default:
      NOTREACHED();
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

void ManagedPrefsBannerBase::AddPref(const wchar_t* pref) {
  if (!relevant_prefs_.count(pref)) {
    if (prefs_->FindPreference(pref)) {
      prefs_->AddPrefObserver(pref, this);
      relevant_prefs_.insert(pref);
    }
  }
}

void ManagedPrefsBannerBase::RemovePref(const wchar_t* pref) {
  PrefSet::iterator iter = relevant_prefs_.find(pref);
  if (iter != relevant_prefs_.end()) {
    prefs_->RemovePrefObserver(pref, this);
    relevant_prefs_.erase(iter);
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
