// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace chrome {

void RegisterBrowserPrefs(PrefServiceSimple* prefs) {
  prefs->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  prefs->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  prefs->RegisterIntegerPref(prefs::kShowFirstRunBubbleOption,
                             first_run::FIRST_RUN_BUBBLE_DONT_SHOW);
}

void RegisterBrowserUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kHomePageChanged,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton,
                             false,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kExtensionsSideloadWipeoutBubbleShown,
                             0,
                             PrefServiceSyncable::SYNCABLE_PREF);
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
                             PrefServiceSyncable::UNSYNCABLE_PREF);
#endif
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCache,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords,
                             false,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData,
                             false,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteHostedAppsData,
                             false,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod,
                             0,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCheckDefaultBrowser,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
#if defined(OS_WIN)
  // As with Mac-spacific code above, it should be in a platform-specific
  // section somewhere, but there is no good place for it.
  prefs->RegisterBooleanPref(prefs::kSuppressSwitchToMetroModeOnSetDefault,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
#endif
  prefs->RegisterBooleanPref(prefs::kShowOmniboxSearchHint,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableTranslate,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kCloudPrintEmail,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDevToolsDisabled,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsHSplitLocation,
                             -1,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsVSplitLocation,
                             -1,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement,
                                PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPreferencesWindowPlacement,
                                PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportBookmarks,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHistory,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHomepage,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSearchEngine,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSavedPasswords,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableDoNotTrack,
                             false,
                             PrefServiceSyncable::SYNCABLE_PREF);

  // Dictionaries to keep track of default tasks in the file browser.
  prefs->RegisterDictionaryPref(prefs::kDefaultTasksByMimeType,
                                PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kDefaultTasksBySuffix,
                                PrefServiceSyncable::SYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  prefs->RegisterBooleanPref(prefs::kPluginsAllowOutdated,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

void RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // We need to register the window position pref.
  //
  // TODO(mnissler): Use a separate pref name pointing to a single
  // dictionary instead. Also tracked as http://crbug.com/167256
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  PrefServiceSyncable* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(window_pref.c_str())) {
    // TODO(joi): Switch to official way of registering local prefs
    // for this class, i.e. in a function called from
    // browser_prefs::RegisterUserPrefs.
    prefs->RegisterDictionaryPref(window_pref.c_str(),
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
  }
}


}  // namespace chrome
