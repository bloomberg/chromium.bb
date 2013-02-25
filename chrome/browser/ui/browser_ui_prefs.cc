// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace chrome {

void RegisterBrowserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  registry->RegisterIntegerPref(prefs::kShowFirstRunBubbleOption,
                             first_run::FIRST_RUN_BUBBLE_DONT_SHOW);
}

void RegisterBrowserUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kHomePageChanged,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kHomePageIsNewTabPage,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kShowHomeButton,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kExtensionsSideloadWipeoutBubbleShown,
                                0,
                                PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
  registry->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(prefs::kDeleteBrowsingHistory,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeleteDownloadHistory,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeleteCache,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeleteCookies,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeletePasswords,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeleteFormData,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDeleteHostedAppsData,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kDeleteTimePeriod,
                                0,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterInt64Pref(prefs::kLastClearBrowsingDataTime,
                              0,
                              PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kCheckDefaultBrowser,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_WIN)
  // As with Mac-spacific code above, it should be in a platform-specific
  // section somewhere, but there is no good place for it.
  registry->RegisterBooleanPref(prefs::kSuppressSwitchToMetroModeOnSetDefault,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(prefs::kShowOmniboxSearchHint,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kEnableTranslate,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kCloudPrintEmail,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDevToolsDisabled,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kDevToolsHSplitLocation,
                                -1,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kDevToolsVSplitLocation,
                                -1,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacement,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kPreferencesWindowPlacement,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kImportBookmarks,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kImportHistory,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kImportHomepage,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kImportSearchEngine,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kImportSavedPasswords,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kEnableDoNotTrack,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);

  // Dictionaries to keep track of default tasks in the file browser.
  registry->RegisterDictionaryPref(prefs::kDefaultTasksByMimeType,
                                   PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kDefaultTasksBySuffix,
                                   PrefRegistrySyncable::SYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  registry->RegisterBooleanPref(prefs::kPluginsAllowOutdated,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kHideWebStoreIcon,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // We need to register the window position pref.
  //
  // TODO(mnissler): Use a separate pref name pointing to a single
  // dictionary instead. Also tracked as http://crbug.com/167256
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(window_pref.c_str())) {
    // TODO(joi): Do all registration up front.
    scoped_refptr<PrefRegistrySyncable> registry(
        static_cast<PrefRegistrySyncable*>(prefs->DeprecatedGetPrefRegistry()));
    registry->RegisterDictionaryPref(window_pref.c_str(),
                                     PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}


}  // namespace chrome
