// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/options/options_util.h"

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/browser/host_zoom_map.h"

// static
void OptionsUtil::ResetToDefaults(Profile* profile) {
  // TODO(tc): It would be nice if we could generate this list automatically so
  // changes to any of the options pages doesn't require updating this list
  // manually.
  PrefService* prefs = profile->GetPrefs();
  const char* kUserPrefs[] = {
    prefs::kAcceptLanguages,
    prefs::kAlternateErrorPagesEnabled,
    prefs::kClearSiteDataOnExit,
    prefs::kCookieBehavior,
    prefs::kDefaultCharset,
    prefs::kDefaultZoomLevel,
    prefs::kDeleteBrowsingHistory,
    prefs::kDeleteCache,
    prefs::kDeleteCookies,
    prefs::kDeleteDownloadHistory,
    prefs::kDeleteFormData,
    prefs::kDeletePasswords,
    prefs::kDnsPrefetchingEnabled,
#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_OPENBSD)
    prefs::kCertRevocationCheckingEnabled,
    prefs::kSSL3Enabled,
    prefs::kTLS1Enabled,
#endif
#if defined(OS_CHROMEOS)
    prefs::kTapToClickEnabled,
    prefs::kTouchpadSensitivity,
#endif
    prefs::kDownloadDefaultDirectory,
    prefs::kDownloadExtensionsToOpen,
    prefs::kSavingBrowserHistoryDisabled,
    prefs::kEnableSpellCheck,
    prefs::kEnableTranslate,
    prefs::kAutoFillEnabled,
    prefs::kAutoFillAuxiliaryProfilesEnabled,
    prefs::kHomePage,
    prefs::kHomePageIsNewTabPage,
    prefs::kPromptForDownload,
    prefs::kPasswordManagerEnabled,
    prefs::kRestoreOnStartup,
    prefs::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingReportingEnabled,
    prefs::kSearchSuggestEnabled,
    prefs::kShowHomeButton,
    prefs::kSpellCheckDictionary,
    prefs::kURLsToRestoreOnStartup,
    prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitDefaultFontSize,
    prefs::kWebKitFixedFontFamily,
    prefs::kWebKitJavaEnabled,
    prefs::kWebKitJavascriptEnabled,
    prefs::kWebKitLoadsImagesAutomatically,
    prefs::kWebKitPluginsEnabled,
    prefs::kWebKitSansSerifFontFamily,
    prefs::kWebKitSerifFontFamily,
    prefs::kWebKitMinimumFontSize,
    prefs::kWebKitMinimumLogicalFontSize,
    prefs::kWebkitTabsToLinks,
  };
  profile->GetDownloadManager()->download_prefs()->ResetToDefaults();
  profile->GetHostContentSettingsMap()->ResetToDefaults();
  profile->GetGeolocationContentSettingsMap()->ResetToDefault();
  profile->GetHostZoomMap()->ResetToDefaults();
  profile->GetDesktopNotificationService()->ResetToDefaultContentSetting();
  for (size_t i = 0; i < arraysize(kUserPrefs); ++i)
    prefs->ClearPref(kUserPrefs[i]);

  PrefService* local_state = g_browser_process->local_state();
  // Note that we don't reset the kMetricsReportingEnabled preference here
  // because the reset will reset it to the default setting specified in Chrome
  // source, not the default setting selected by the user on the web page where
  // they downloaded Chrome. This means that if the user ever resets their
  // settings they'll either inadvertedly enable this logging or disable it.
  // One is undesirable for them, one is undesirable for us. For now, we just
  // don't reset it.
  const char* kLocalStatePrefs[] = {
    prefs::kApplicationLocale,
  };
  for (size_t i = 0; i < arraysize(kLocalStatePrefs); ++i)
    local_state->ClearPref(kLocalStatePrefs[i]);
}

// static
bool OptionsUtil::ResolveMetricsReportingEnabled(bool enabled) {
  // GoogleUpdateSettings touches the disk from the UI thread. MetricsService
  // also calls GoogleUpdateSettings below. http://crbug/62626
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();

  if (enabled != update_pref) {
    DVLOG(1) << "OptionsUtil: Unable to set crash report status to " << enabled;
  }

  // Only change the pref if GoogleUpdateSettings::GetCollectStatsConsent
  // succeeds.
  enabled = update_pref;

  MetricsService* metrics = g_browser_process->metrics_service();
  DCHECK(metrics);
  if (metrics) {
    metrics->SetUserPermitsUpload(enabled);
    if (enabled)
      metrics->Start();
    else
      metrics->Stop();
  }

  return enabled;
}
