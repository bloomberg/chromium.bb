// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a list of the preferences that the
// PreferencesChangeProcessor should process changes for.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_

#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

static const wchar_t* kSynchronizedPreferences[] = {
  // Options dialog: Basics tab.
  prefs::kRestoreOnStartup,
  prefs::kURLsToRestoreOnStartup,
  prefs::kShowBookmarkBar,
  prefs::kHomePageIsNewTabPage,
  prefs::kHomePage,
  prefs::kShowHomeButton,
  prefs::kShowPageOptionsButtons,
  // Default Search is not synced, needs a new data type.  See
  // http://crbug.com/40482

  // Options dialog: Personal Stuff tab.
  prefs::kPasswordManagerEnabled,
  prefs::kAutoFillEnabled,
  prefs::kUseCustomChromeFrame,

  // Options dialog: Under the hood -> Content Settings -> Cookies.
  //   Cookie settings and exceptions not working
  prefs::kBlockThirdPartyCookies,
  prefs::kClearSiteDataOnExit,

  // Options dialog: Under the hood -> Content Settings ->
  //     Images, JavaScript, Plug-ins, Pop-ups.
  prefs::kDefaultContentSettings,
  prefs::kContentSettingsPatterns,

  // Options dialog: Under the hood -> Content Settings -> Location.
  //   Exceptions not working (dialog not working either).
  prefs::kGeolocationContentSettings,
  prefs::kGeolocationDefaultContentSetting,

  // Options dialog: under the hood -> Content Settings -> Notifications.
  prefs::kDesktopNotificationDefaultContentSetting,

  // Options dialog: Under the hood -> Clear browsing data.
  //  All working but no live update.
  prefs::kDeleteBrowsingHistory,
  prefs::kDeleteDownloadHistory,
  prefs::kDeleteCache,
  prefs::kDeleteCookies,
  prefs::kDeletePasswords,
  prefs::kDeleteFormData,
  prefs::kDeleteTimePeriod,

  // Options dialog: Under the hood -> Change proxy settings.
  //  Uses native OS dialog, not synced.

  // Options dialog: Under the hood -> Change font and language settings.
  //   Serif, San Serif, Fixed font settings not synced.
  prefs::kDefaultCharset,
  // There is no dialog to modify the kAcceptLanguages list on OSX, so
  // don't sync it.
#if !defined(OS_MACOSX)
  prefs::kAcceptLanguages,
#endif
  prefs::kEnableSpellCheck,
  // Spell checker language not synced.
  prefs::kApplicationLocale,

  // Options dialog: Under the hood.
  prefs::kAlternateErrorPagesEnabled,
  prefs::kSearchSuggestEnabled,
  prefs::kDnsPrefetchingEnabled,
  prefs::kSafeBrowsingEnabled,
  prefs::kEnableTranslate,
  // Download directory not synced.
  // Clear auto-opening settings not synced.
  prefs::kPromptForDownload,

  // Wrench menu -> Extensions.
  prefs::kExtensionsUIDeveloperMode,  // no live update

  // Document menu -> Zoom.
  //   prefs::kPerHostZoomLevels creates bad UX when synced, see
  //   http://crbug.com/47359.

  // Document menu -> Encoding -> Auto Detect.
  prefs::kWebKitUsesUniversalDetector,

  // Autofill dialog.
  prefs::kAutoFillAuxiliaryProfilesEnabled,

  // Translate preferences.
  TranslatePrefs::kPrefTranslateLanguageBlacklist,
  TranslatePrefs::kPrefTranslateSiteBlacklist,
  TranslatePrefs::kPrefTranslateWhitelists,
  TranslatePrefs::kPrefTranslateDeniedCount,
  TranslatePrefs::kPrefTranslateAcceptedCount,

  // Desktop notification permissions.
  prefs::kDesktopNotificationAllowedOrigins,
  prefs::kDesktopNotificationDeniedOrigins,

  // Cookie prompt dialog.
  prefs::kCookiePromptExpanded,

#if defined(OS_CHROMEOS)
  // IME prefs
  prefs::kLanguageHangulKeyboard,
  prefs::kLanguagePinyinAutoCommit,
  prefs::kLanguagePinyinCommaPeriodPage,
  prefs::kLanguagePinyinCorrectPinyin,
  prefs::kLanguagePinyinDoublePinyin,
  prefs::kLanguagePinyinFuzzyPinyin,
  prefs::kLanguagePinyinInitChinese,
  prefs::kLanguagePinyinInitFull,
  prefs::kLanguagePinyinInitFullPunct,
  prefs::kLanguagePinyinInitSimplifiedChinese,
  prefs::kLanguagePinyinMinusEqualPage,
  prefs::kLanguagePinyinShiftSelectCandidate,
  prefs::kLanguagePinyinTradCandidate,
  prefs::kLanguagePreferredLanguages,
  prefs::kLanguagePreloadEngines,
  // TODO(yusukes): Add prefs for ibus-mozc (Japanese input method).
  //   prefs::kLanguageMozcPreeditMethod,
  //   prefs::kLanguageMozcSessionKeymap,
  //   prefs::kLanguageMozcPunctuationMethod,
  //   prefs::kLanguageMozcSymbolMethod,
  //   prefs::kLanguageMozcSpaceCharacterForm;
  //   prefs::kLanguageMozcHistoryLearningLevel;
  //   prefs::kLanguageMozcSelectionShortcut;
  //   prefs::kLanguageMozcShiftKeyModeSwitch;
  //   prefs::kLanguageMozcNumpadCharacterForm;
  //   prefs::kLanguageMozcIncognitoMode,
  //   prefs::kLanguageMozcUseAutoImeTurnOff,
  //   prefs::kLanguageMozcUseDateConversion,
  //   prefs::kLanguageMozcUseSingleKanjiConversion,
  //   prefs::kLanguageMozcUseSymbolConversion,
  //   prefs::kLanguageMozcUseNumberConversion,
  //   prefs::kLanguageMozcUseHistorySuggest,
  //   prefs::kLanguageMozcUseDictionarySuggest,
  //   prefs::kLanguageMozcSuggestionsSize
  //
  // We don't sync the following IME prefs since they are not user-configurable
  // (yet):
  //   prefs::kLanguageHotkeyNextEngineInMenu,
  //   prefs::kLanguageHotkeyPreviousEngine,
  //   prefs::kLanguagePinyinDoublePinyinSchema,
  //   prefs::kLanguagePinyinLookupTablePageSize,
  //
  // We don't sync prefs::kLanguageCurrentInputMethod and PreviousInputMethod.
#endif
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCHRONIZED_PREFERENCES_H_
