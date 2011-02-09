// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_prefs_banner_base.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

namespace policy {

ManagedPrefsBannerBase::ManagedPrefsBannerBase(PrefService* user_prefs,
                                               OptionsPage page) {
  Init(g_browser_process->local_state(), user_prefs, page);
}

ManagedPrefsBannerBase::ManagedPrefsBannerBase(PrefService* local_state,
                                               PrefService* user_prefs,
                                               OptionsPage page) {
  Init(local_state, user_prefs, page);
}

ManagedPrefsBannerBase::~ManagedPrefsBannerBase() {}

void ManagedPrefsBannerBase::AddLocalStatePref(const char* pref) {
  local_state_set_->AddPref(pref);
}

void ManagedPrefsBannerBase::RemoveLocalStatePref(const char* pref) {
  local_state_set_->RemovePref(pref);
}

void ManagedPrefsBannerBase::AddUserPref(const char* pref) {
  user_pref_set_->AddPref(pref);
}

void ManagedPrefsBannerBase::RemoveUserPref(const char* pref) {
  user_pref_set_->RemovePref(pref);
}

bool ManagedPrefsBannerBase::DetermineVisibility() const {
  return local_state_set_->IsManaged() || user_pref_set_->IsManaged();
}

void ManagedPrefsBannerBase::Init(PrefService* local_state,
                                  PrefService* user_prefs,
                                  OptionsPage page) {
  local_state_set_.reset(new PrefSetObserver(local_state, this));
  user_pref_set_.reset(new PrefSetObserver(user_prefs, this));

  switch (page) {
    case OPTIONS_PAGE_GENERAL:
      AddUserPref(prefs::kHomePage);
      AddUserPref(prefs::kHomePageIsNewTabPage);
      AddUserPref(prefs::kShowHomeButton);
      AddUserPref(prefs::kRestoreOnStartup);
      AddUserPref(prefs::kURLsToRestoreOnStartup);
      AddUserPref(prefs::kDefaultSearchProviderEnabled);
      AddUserPref(prefs::kDefaultSearchProviderName);
      AddUserPref(prefs::kDefaultSearchProviderKeyword);
      AddUserPref(prefs::kDefaultSearchProviderInstantURL);
      AddUserPref(prefs::kDefaultSearchProviderSearchURL);
      AddUserPref(prefs::kDefaultSearchProviderSuggestURL);
      AddUserPref(prefs::kDefaultSearchProviderIconURL);
      AddUserPref(prefs::kDefaultSearchProviderEncodings);
      AddUserPref(prefs::kInstantEnabled);
      AddLocalStatePref(prefs::kDefaultBrowserSettingEnabled);
      break;
    case OPTIONS_PAGE_CONTENT:
      AddUserPref(prefs::kSyncManaged);
      AddUserPref(prefs::kAutoFillEnabled);
      AddUserPref(prefs::kPasswordManagerEnabled);
#if defined(OS_CHROMEOS)
      AddUserPref(prefs::kEnableScreenLock);
#endif
      break;
    case OPTIONS_PAGE_ADVANCED:
      AddUserPref(prefs::kAlternateErrorPagesEnabled);
      AddUserPref(prefs::kSearchSuggestEnabled);
      AddUserPref(prefs::kDnsPrefetchingEnabled);
      AddUserPref(prefs::kDisableSpdy);
      AddUserPref(prefs::kSafeBrowsingEnabled);
#if defined(GOOGLE_CHROME_BUILD)
      AddLocalStatePref(prefs::kMetricsReportingEnabled);
#endif
      AddUserPref(prefs::kProxy);
      AddUserPref(prefs::kCloudPrintProxyEnabled);
      break;
    default:
      NOTREACHED();
  }
}

void ManagedPrefsBannerBase::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    std::string* pref = Details<std::string>(details).ptr();
    if (pref && (local_state_set_->IsObserved(*pref) ||
                 user_pref_set_->IsObserved(*pref)))
      OnUpdateVisibility();
  }
}

}  // namespace policy
