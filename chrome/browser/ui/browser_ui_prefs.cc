// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace chrome {

void SetNewHomePagePrefs(PrefService* prefs) {
  const PrefService::Preference* home_page_pref =
      prefs->FindPreference(prefs::kHomePage);
  if (home_page_pref &&
      !home_page_pref->IsManaged() &&
      !prefs->HasPrefPath(prefs::kHomePage)) {
    prefs->SetString(prefs::kHomePage, std::string());
  }
  const PrefService::Preference* home_page_is_new_tab_page_pref =
      prefs->FindPreference(prefs::kHomePageIsNewTabPage);
  if (home_page_is_new_tab_page_pref &&
      !home_page_is_new_tab_page_pref->IsManaged() &&
      !prefs->HasPrefPath(prefs::kHomePageIsNewTabPage))
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
}

void RegisterBrowserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  prefs->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  prefs->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
}

void RegisterBrowserUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kHomePageChanged,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton,
                             false,
                             PrefService::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
  prefs->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCache,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteHostedAppsData,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod,
                             0,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCheckDefaultBrowser,
                             true,
                             PrefService::UNSYNCABLE_PREF);
#if defined(OS_WIN)
  // As with Mac-spacific code above, it should be in a platform-specific
  // section somewhere, but there is no good place for it.
  prefs->RegisterBooleanPref(prefs::kSuppressSwitchToMetroModeOnSetDefault,
                             false,
                             PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterBooleanPref(prefs::kShowOmniboxSearchHint,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableTranslate,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kCloudPrintEmail,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDevToolsDisabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsHSplitLocation,
                             -1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsVSplitLocation,
                             -1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPreferencesWindowPlacement,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportBookmarks,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHistory,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHomepage,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSearchEngine,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSavedPasswords,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableDoNotTrack,
                             false,
                             PrefService::SYNCABLE_PREF);

  // Dictionaries to keep track of default tasks in the file browser.
  prefs->RegisterDictionaryPref(prefs::kDefaultTasksByMimeType,
                                PrefService::SYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kDefaultTasksBySuffix,
                                PrefService::SYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  prefs->RegisterBooleanPref(prefs::kPluginsAllowOutdated,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

void RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // We need to register the window position pref.
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(window_pref.c_str())) {
    prefs->RegisterDictionaryPref(window_pref.c_str(),
                                  PrefService::UNSYNCABLE_PREF);
  }
}


}  // namespace chrome
