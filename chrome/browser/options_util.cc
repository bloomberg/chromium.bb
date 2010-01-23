// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/options_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/installer/util/google_update_settings.h"

// static
void OptionsUtil::ResetToDefaults(Profile* profile) {
  // TODO(tc): It would be nice if we could generate this list automatically so
  // changes to any of the options pages doesn't require updating this list
  // manually.
  PrefService* prefs = profile->GetPrefs();
  const wchar_t* kUserPrefs[] = {
    prefs::kAcceptLanguages,
    prefs::kAlternateErrorPagesEnabled,
    prefs::kCookieBehavior,
    prefs::kDefaultCharset,
    prefs::kDnsPrefetchingEnabled,
#if defined(OS_LINUX)
    prefs::kCertRevocationCheckingEnabled,
    prefs::kSSL2Enabled,
    prefs::kSSL3Enabled,
    prefs::kTLS1Enabled,
#endif
#if defined(OS_CHROMEOS)
    prefs::kTimeZone,
    prefs::kTapToClickEnabled,
    prefs::kVertEdgeScrollEnabled,
    prefs::kTouchpadSpeedFactor,
    prefs::kTouchpadSensitivity,
#endif
    prefs::kDownloadDefaultDirectory,
    prefs::kDownloadExtensionsToOpen,
    prefs::kEnableSpellCheck,
    prefs::kFormAutofillEnabled,
    prefs::kHomePage,
    prefs::kHomePageIsNewTabPage,
    prefs::kMixedContentFiltering,
    prefs::kPrivacyFilterRules,
    prefs::kPromptForDownload,
    prefs::kPasswordManagerEnabled,
    prefs::kRestoreOnStartup,
    prefs::kSafeBrowsingEnabled,
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
  };
  profile->GetDownloadManager()->ResetAutoOpenFiles();
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
  const wchar_t* kLocalStatePrefs[] = {
    prefs::kApplicationLocale,
    prefs::kOptionsWindowLastTabIndex,
  };
  for (size_t i = 0; i < arraysize(kLocalStatePrefs); ++i)
    local_state->ClearPref(kLocalStatePrefs[i]);
}

// static
bool OptionsUtil::ResolveMetricsReportingEnabled(bool enabled) {
  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();

  if (enabled != update_pref) {
    DLOG(INFO) <<
        "OptionsUtil: Unable to set crash report status to " <<
        enabled;
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
