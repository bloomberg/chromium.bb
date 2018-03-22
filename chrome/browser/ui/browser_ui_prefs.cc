// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "media/media_buildflags.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

uint32_t GetHomeButtonAndHomePageIsNewTabPageFlags() {
#if defined(OS_ANDROID)
  return PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  return user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
}

}  // namespace

void RegisterBrowserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(prefs::kRelaunchNotification, 0);
#endif  // !defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(
      prefs::kRelaunchNotificationPeriod,
      base::saturated_cast<int>(
          UpgradeDetector::GetDefaultHighAnnoyanceThreshold()
              .InMilliseconds()));
#endif  // !defined(OS_ANDROID)
}

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kHomePageIsNewTabPage,
      true,
      GetHomeButtonAndHomePageIsNewTabPageFlags());
  registry->RegisterBooleanPref(prefs::kShowHomeButton, false,
                                GetHomeButtonAndHomePageIsNewTabPageFlags());

  registry->RegisterIntegerPref(prefs::kModuleConflictBubbleShown, 0);
  registry->RegisterInt64Pref(prefs::kDefaultBrowserLastDeclined, 0);
  bool reset_check_default = false;
#if defined(OS_WIN)
  reset_check_default = base::win::GetVersion() >= base::win::VERSION_WIN10;
#endif
  registry->RegisterBooleanPref(prefs::kResetCheckDefaultBrowser,
                                reset_check_default);
  registry->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar, true);
  registry->RegisterBooleanPref(
      prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kCloudPrintEmail, std::string());
  registry->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, true);
  registry->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled, true);
  registry->RegisterBooleanPref(prefs::kDevToolsDisabled, false);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacementPopup);
  registry->RegisterDictionaryPref(prefs::kAppWindowPlacement);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kPrintPreviewUseSystemDefaultPrinter,
                                false);
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
  // TODO(guoweis): Remove next 2 options at M50.
  registry->RegisterBooleanPref(prefs::kWebRTCMultipleRoutesEnabled, true);
  registry->RegisterBooleanPref(prefs::kWebRTCNonProxiedUdpEnabled, true);
  registry->RegisterStringPref(prefs::kWebRTCIPHandlingPolicy,
                               content::kWebRTCIPHandlingDefault);
  registry->RegisterStringPref(prefs::kWebRTCUDPPortRange, std::string());
#endif

  // Dictionaries to keep track of default tasks in the file browser.
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  registry->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled, true);
  registry->RegisterBooleanPref(prefs::kHideWebStoreIcon, false);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
  registry->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar, true);
  registry->RegisterBooleanPref(
      prefs::kShowFullscreenToolbar,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAllowJavascriptAppleEvents, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#else
  registry->RegisterBooleanPref(prefs::kFullscreenAllowed, true);
#endif
}
