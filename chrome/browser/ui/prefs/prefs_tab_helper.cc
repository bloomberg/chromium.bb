// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/prefs/prefs_tab_helper.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/overlay_user_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "webkit/glue/webpreferences.h"

using content::WebContents;

namespace {

const char* kPerTabPrefsToObserve[] = {
  prefs::kDefaultCharset,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitCursiveFontFamily,
  prefs::kWebKitFantasyFontFamily,
  prefs::kWebKitFixedFontFamily,
  prefs::kWebKitSansSerifFontFamily,
  prefs::kWebKitSerifFontFamily,
  prefs::kWebKitStandardFontFamily,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize
};

const int kPerTabPrefsToObserveLength = arraysize(kPerTabPrefsToObserve);

static void RegisterFontsAndCharsetPrefs(PrefService* prefs) {
  WebPreferences pref_defaults;

  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumLogicalFontSize,
                                      IDS_MINIMUM_LOGICAL_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
}

static void RegisterPerTabUserPrefs(PrefService* prefs) {
  WebPreferences pref_defaults;

  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      true,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                             pref_defaults.plugins_enabled,
                             PrefService::UNSYNCABLE_PREF);
  RegisterFontsAndCharsetPrefs(prefs);
}

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kDefaultZoomLevel,
  prefs::kGlobalDefaultCharset,
  prefs::kEnableReferrers,
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitGlobalCursiveFontFamily,
  prefs::kWebKitGlobalDefaultFixedFontSize,
  prefs::kWebKitGlobalDefaultFontSize,
  prefs::kWebKitGlobalFantasyFontFamily,
  prefs::kWebKitGlobalFixedFontFamily,
  prefs::kWebKitGlobalJavascriptEnabled,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitGlobalLoadsImagesAutomatically,
  prefs::kWebKitGlobalMinimumFontSize,
  prefs::kWebKitGlobalMinimumLogicalFontSize,
  prefs::kWebKitGlobalPluginsEnabled,
  prefs::kWebKitGlobalSansSerifFontFamily,
  prefs::kWebKitGlobalSerifFontFamily,
  prefs::kWebKitGlobalStandardFontFamily,
  prefs::kWebkitTabsToLinks,
  prefs::kWebKitUsesUniversalDetector
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

// Registers a preference under the path |map_name| for each script used for
// per-script font prefs.  For example, if |map_name| is "fonts.serif", then
// "fonts.serif.Arab", "fonts.serif.Hang", etc. are registered.
void RegisterFontFamilyMap(PrefService* prefs, const char* map_name) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name_str = base::StringPrintf("%s.%s", map_name, script);
    const char* pref_name = pref_name_str.c_str();
    if (!prefs->FindPreference(pref_name))
      prefs->RegisterStringPref(pref_name, "", PrefService::UNSYNCABLE_PREF);
  }
}

// Registers |obs| to observe per-script font prefs under the path |map_name|.
void RegisterFontFamilyMapObserver(PrefChangeRegistrar* registrar,
                                   const char* map_name,
                                   content::NotificationObserver* obs) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    registrar->Add(pref_name.c_str(), obs);
  }
}

struct PerScriptFontDefault {
  const char* pref_name;
  int resource_id;
  const char* native_locale;
};

// Per-script font pref defaults.  The prefs that have defaults vary by
// platform, since not all platforms have fonts for all scripts for all generic
// families.
// TODO(falken): add proper defaults when possible for all
// platforms/scripts/generic families.
const PerScriptFontDefault kPerScriptFontDefaults[] = {
#if defined(OS_CHROMEOS)
  { prefs::kWebKitStandardFontFamilyArabic, IDS_STANDARD_FONT_FAMILY_ARABIC,
    "ar" },
  { prefs::kWebKitSerifFontFamilyArabic, IDS_SERIF_FONT_FAMILY_ARABIC, "ar" },
  { prefs::kWebKitSansSerifFontFamilyArabic,
    IDS_SANS_SERIF_FONT_FAMILY_ARABIC, "ar" },
  { prefs::kWebKitStandardFontFamilyJapanese,
    IDS_STANDARD_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitFixedFontFamilyJapanese, IDS_FIXED_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSerifFontFamilyJapanese, IDS_SERIF_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSansSerifFontFamilyJapanese,
    IDS_SANS_SERIF_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitStandardFontFamilyKorean, IDS_STANDARD_FONT_FAMILY_KOREAN,
    "ko" },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitSerifFontFamilyKorean, IDS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitSansSerifFontFamilyKorean,
    IDS_SANS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitStandardFontFamilySimplifiedHan,
    IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSerifFontFamilySimplifiedHan,
    IDS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSansSerifFontFamilySimplifiedHan,
    IDS_SANS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitStandardFontFamilyTraditionalHan,
    IDS_STANDARD_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSerifFontFamilyTraditionalHan,
    IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSansSerifFontFamilyTraditionalHan,
    IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" }
#elif defined(OS_MACOSX)
  { prefs::kWebKitStandardFontFamilyJapanese,
    IDS_STANDARD_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitFixedFontFamilyJapanese, IDS_FIXED_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSerifFontFamilyJapanese, IDS_SERIF_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSansSerifFontFamilyJapanese,
    IDS_SANS_SERIF_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitStandardFontFamilyKorean, IDS_STANDARD_FONT_FAMILY_KOREAN,
    "ko" },
  { prefs::kWebKitSerifFontFamilyKorean, IDS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitSansSerifFontFamilyKorean,
    IDS_SANS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitStandardFontFamilySimplifiedHan,
    IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSerifFontFamilySimplifiedHan,
    IDS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSansSerifFontFamilySimplifiedHan,
    IDS_SANS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitStandardFontFamilyTraditionalHan,
    IDS_STANDARD_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSerifFontFamilyTraditionalHan,
    IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSansSerifFontFamilyTraditionalHan,
    IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" }
#elif defined(OS_WIN)
  { prefs::kWebKitStandardFontFamilyJapanese,
    IDS_STANDARD_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitFixedFontFamilyJapanese, IDS_FIXED_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSerifFontFamilyJapanese, IDS_SERIF_FONT_FAMILY_JAPANESE,
    "ja" },
  { prefs::kWebKitSansSerifFontFamilyJapanese,
    IDS_SANS_SERIF_FONT_FAMILY_JAPANESE, "ja" },
  { prefs::kWebKitStandardFontFamilyKorean, IDS_STANDARD_FONT_FAMILY_KOREAN,
    "ko" },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitSerifFontFamilyKorean, IDS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitSansSerifFontFamilyKorean,
    IDS_SANS_SERIF_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitCursiveFontFamilyKorean, IDS_CURSIVE_FONT_FAMILY_KOREAN,
    "ko" },
  { prefs::kWebKitStandardFontFamilySimplifiedHan,
    IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSerifFontFamilySimplifiedHan,
    IDS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitSansSerifFontFamilySimplifiedHan,
    IDS_SANS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitStandardFontFamilyTraditionalHan,
    IDS_STANDARD_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSerifFontFamilyTraditionalHan,
    IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
  { prefs::kWebKitSansSerifFontFamilyTraditionalHan,
    IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" }
#endif
};

#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_WIN)
// To avoid Clang warning, only define kPerScriptFontDefaultsLength when it is
// non-zero.  When it is zero, code like
//  for (size_t i = 0; i < kPerScriptFontDefaultsLength; ++i)
// causes a warning due to comparison of unsigned expression < 0.
const size_t kPerScriptFontDefaultsLength = arraysize(kPerScriptFontDefaults);
#endif

const struct {
  const char* from;
  const char* to;
} kPrefNamesToMigrate[] = {
  { prefs::kDefaultCharset,
    prefs::kGlobalDefaultCharset },
  { prefs::kWebKitCursiveFontFamily,
    prefs::kWebKitGlobalCursiveFontFamily },
  { prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitGlobalDefaultFixedFontSize },
  { prefs::kWebKitDefaultFontSize,
    prefs::kWebKitGlobalDefaultFontSize },
  { prefs::kWebKitFantasyFontFamily,
    prefs::kWebKitGlobalFantasyFontFamily },
  { prefs::kWebKitFixedFontFamily,
    prefs::kWebKitGlobalFixedFontFamily },
  { prefs::kWebKitMinimumFontSize,
    prefs::kWebKitGlobalMinimumFontSize },
  { prefs::kWebKitMinimumLogicalFontSize,
    prefs::kWebKitGlobalMinimumLogicalFontSize },
  { prefs::kWebKitSansSerifFontFamily,
    prefs::kWebKitGlobalSansSerifFontFamily },
  { prefs::kWebKitSerifFontFamily,
    prefs::kWebKitGlobalSerifFontFamily },
  { prefs::kWebKitStandardFontFamily,
    prefs::kWebKitGlobalStandardFontFamily },
};

const int kPrefsToMigrateLength = ARRAYSIZE_UNSAFE(kPrefNamesToMigrate);

static void MigratePreferences(PrefService* prefs) {
  RegisterFontsAndCharsetPrefs(prefs);
  for (int i = 0; i < kPrefsToMigrateLength; ++i) {
    const PrefService::Preference *pref =
        prefs->FindPreference(kPrefNamesToMigrate[i].from);
    if (!pref) continue;
    if (!pref->IsDefaultValue()) {
      prefs->Set(kPrefNamesToMigrate[i].to, *pref->GetValue()->DeepCopy());
    }
    prefs->UnregisterPreference(kPrefNamesToMigrate[i].from);
  }
}

}  // namespace

PrefsTabHelper::PrefsTabHelper(WebContents* contents)
    : content::WebContentsObserver(contents) {
  PrefService* prefs = GetProfile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      pref_change_registrar_.Add(kPrefsToObserve[i], this);

    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitStandardFontFamilyMap, this);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitFixedFontFamilyMap, this);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitSerifFontFamilyMap, this);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitSansSerifFontFamilyMap, this);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitCursiveFontFamilyMap, this);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitFantasyFontFamilyMap, this);
  }

  per_tab_prefs_.reset(
      GetProfile()->GetPrefs()->CreatePrefServiceWithPerTabPrefStore());
  RegisterPerTabUserPrefs(per_tab_prefs_.get());
  per_tab_pref_change_registrar_.Init(per_tab_prefs_.get());
  for (int i = 0; i < kPerTabPrefsToObserveLength; ++i) {
      per_tab_pref_change_registrar_.Add(kPerTabPrefsToObserve[i], this);
  }

  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents()->GetMutableRendererPrefs(), GetProfile());

  registrar_.Add(this, chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED,
                 content::NotificationService::AllSources());
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
#endif
}

PrefsTabHelper::~PrefsTabHelper() {
}

// static
void PrefsTabHelper::InitIncognitoUserPrefStore(
    OverlayUserPrefStore* pref_store) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // profile.  All preferences that store information about the browsing history
  // or behavior of the user should have this property.
  pref_store->RegisterOverlayPref(prefs::kBrowserWindowPlacement);
}

// static
void PrefsTabHelper::InitPerTabUserPrefStore(
    OverlayUserPrefStore* pref_store) {
  // List of keys that have per-tab and per-profile (Global) counterpart.
  // Setting a per-profile preference affects all tabs sharing the profile,
  // unless a tab has specified its own per-tab property value. Changing a
  // per-profile preference on a per-tab pref store delegates to the underlying
  // per-profile pref store.
  pref_store->RegisterOverlayPref(
      prefs::kWebKitJavascriptEnabled,
      prefs::kWebKitGlobalJavascriptEnabled);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      prefs::kWebKitGlobalJavascriptCanOpenWindowsAutomatically);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitLoadsImagesAutomatically,
      prefs::kWebKitGlobalLoadsImagesAutomatically);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitPluginsEnabled,
      prefs::kWebKitGlobalPluginsEnabled);
  pref_store->RegisterOverlayPref(
      prefs::kDefaultCharset,
      prefs::kGlobalDefaultCharset);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitStandardFontFamily,
      prefs::kWebKitGlobalStandardFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitFixedFontFamily,
      prefs::kWebKitGlobalFixedFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitSerifFontFamily,
      prefs::kWebKitGlobalSerifFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitSansSerifFontFamily,
      prefs::kWebKitGlobalSansSerifFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitCursiveFontFamily,
      prefs::kWebKitGlobalCursiveFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitFantasyFontFamily,
      prefs::kWebKitGlobalFantasyFontFamily);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitDefaultFontSize,
      prefs::kWebKitGlobalDefaultFontSize);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitDefaultFixedFontSize,
      prefs::kWebKitGlobalDefaultFixedFontSize);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitMinimumFontSize,
      prefs::kWebKitGlobalMinimumFontSize);
  pref_store->RegisterOverlayPref(
      prefs::kWebKitMinimumLogicalFontSize,
      prefs::kWebKitGlobalMinimumLogicalFontSize);
}

// static
void PrefsTabHelper::RegisterUserPrefs(PrefService* prefs) {
  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitGlobalJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kWebKitGlobalJavascriptCanOpenWindowsAutomatically,
      true,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitGlobalLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitGlobalPluginsEnabled,
                             pref_defaults.plugins_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                             pref_defaults.dom_paste_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitShrinksStandaloneImagesToFit,
                             pref_defaults.shrinks_standalone_images_to_fit,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kWebKitInspectorSettings,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                             pref_defaults.text_areas_are_resizable,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitJavaEnabled,
                             pref_defaults.java_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebkitTabsToLinks,
                             pref_defaults.tabs_to_links,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitAllowRunningInsecureContent,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitAllowDisplayingInsecureContent,
                             true,
                             PrefService::UNSYNCABLE_PREF);

#if !defined(OS_MACOSX)
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::SYNCABLE_PREF);
#else
  // Not used in OSX.
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterLocalizedStringPref(prefs::kGlobalDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitGlobalFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);

  // Register per-script font prefs that have defaults.
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_WIN)
  // As explained by its definition, kPerScriptFontDefaultsLength is only
  // defined for platforms where it would be non-zero.
  std::string locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < kPerScriptFontDefaultsLength; ++i) {
    const PerScriptFontDefault& pref = kPerScriptFontDefaults[i];
    // Suppress default per-script font when the script matches the browser's
    // locale.  Otherwise, the default would override the user's preferences
    // when viewing pages in their native language.  This can be removed when
    // per-script fonts are added to Preferences UI.
    if (!StartsWithASCII(locale, pref.native_locale, false)) {
      prefs->RegisterLocalizedStringPref(pref.pref_name,
                                         pref.resource_id,
                                         PrefService::UNSYNCABLE_PREF);
    }
  }
#endif

  // Register the rest of the per-script font prefs.
  RegisterFontFamilyMap(prefs, prefs::kWebKitStandardFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitFixedFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitSerifFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitSansSerifFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitCursiveFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitFantasyFontFamilyMap);

  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitGlobalDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitGlobalDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitGlobalMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(
      prefs::kWebKitGlobalMinimumLogicalFontSize,
      IDS_MINIMUM_LOGICAL_FONT_SIZE,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR,
                                      PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  MigratePreferences(prefs);
}

void PrefsTabHelper::RenderViewCreated(RenderViewHost* render_view_host) {
  UpdateWebPreferences();
}

void PrefsTabHelper::WebContentsDestroyed(WebContents* tab) {
  per_tab_pref_change_registrar_.RemoveAll();
}

void PrefsTabHelper::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED:
      UpdateWebPreferences();
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      UpdateRendererPreferences();
      break;
    }
#endif
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = content::Details<std::string>(details).ptr();
      DCHECK(content::Source<PrefService>(source).ptr() ==
                 GetProfile()->GetPrefs() ||
             content::Source<PrefService>(source).ptr() == per_tab_prefs_);
      if ((*pref_name_in == prefs::kDefaultCharset ||
           *pref_name_in == prefs::kGlobalDefaultCharset) ||
          StartsWithASCII(*pref_name_in, "webkit.webprefs.", true)) {
        UpdateWebPreferences();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel ||
                 *pref_name_in == prefs::kEnableReferrers) {
        UpdateRendererPreferences();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void PrefsTabHelper::UpdateWebPreferences() {
  content::RenderViewHostDelegate* rvhd =
      web_contents()->GetRenderViewHost()->delegate();
  WebPreferences prefs = rvhd->GetWebkitPrefs();
  prefs.javascript_enabled =
      per_tab_prefs_->GetBoolean(prefs::kWebKitJavascriptEnabled);
  prefs.javascript_can_open_windows_automatically =
      per_tab_prefs_->GetBoolean(
          prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  prefs.loads_images_automatically =
      per_tab_prefs_->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);
  prefs.plugins_enabled =
      per_tab_prefs_->GetBoolean(prefs::kWebKitPluginsEnabled);
  prefs.standard_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitStandardFontFamily));
  prefs.fixed_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitFixedFontFamily));
  prefs.serif_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitSerifFontFamily));
  prefs.sans_serif_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitSansSerifFontFamily));
  prefs.cursive_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitCursiveFontFamily));
  prefs.fantasy_font_family =
      UTF8ToUTF16(per_tab_prefs_->GetString(prefs::kWebKitFantasyFontFamily));
  prefs.default_font_size =
      per_tab_prefs_->GetInteger(prefs::kWebKitDefaultFontSize);
  prefs.default_fixed_font_size =
      per_tab_prefs_->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  prefs.minimum_font_size =
      per_tab_prefs_->GetInteger(prefs::kWebKitMinimumFontSize);
  prefs.minimum_logical_font_size =
      per_tab_prefs_->GetInteger(prefs::kWebKitMinimumLogicalFontSize);
  prefs.default_encoding =
      per_tab_prefs_->GetString(prefs::kDefaultCharset);

  web_contents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
}

void PrefsTabHelper::UpdateRendererPreferences() {
  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents()->GetMutableRendererPrefs(), GetProfile());
  web_contents()->GetRenderViewHost()->SyncRendererPrefs();
}

Profile* PrefsTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}
