// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/prefs/prefs_tab_helper.h"

#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "webkit/glue/webpreferences.h"

using content::WebContents;
using webkit_glue::WebPreferences;

namespace {

// Registers prefs only used for migration.
static void RegisterPrefsToMigrate(PrefService* prefs) {
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitOldFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kGlobalDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);
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
}

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kDefaultZoomLevel,
  prefs::kDefaultCharset,
  prefs::kEnableReferrers,
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize,
  prefs::kWebKitPluginsEnabled,
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

struct FontDefault {
  const char* pref_name;
  int resource_id;

  // The locale that matches the script this default pref is for.  May be a
  // comma-separated list.  For example, for a Cyrillic font pref,
  // |native_locale| is something like "sr,ru" (Serbian and Russian).  When the
  // locale of the browser process is in |native_locale|, the default is not
  // registered to avoid overriding the user's font pref (see comments in
  // PrefTabsHelper::RegisterUserPrefs).  When |native_locale| is the empty
  // string, the default is registered regardless of locale.
  const char* native_locale;
};

// Locales that match Cyrllic script, according to ICU data. Only define
// |kCyrllicLocales| on platforms it's used on to avoid compiler error.
#if defined(OS_WIN)
const char* kCyrllicLocales = "be,bg,kk,mk,ru,sr,uk,uz";
#endif

// Font pref defaults.  The prefs that have defaults vary by platform, since not
// all platforms have fonts for all scripts for all generic families.
// TODO(falken): add proper defaults when possible for all
// platforms/scripts/generic families.
const FontDefault kFontDefaults[] = {
  { prefs::kWebKitStandardFontFamily, IDS_STANDARD_FONT_FAMILY, "" },
  { prefs::kWebKitFixedFontFamily, IDS_FIXED_FONT_FAMILY, "" },
  { prefs::kWebKitSerifFontFamily, IDS_SERIF_FONT_FAMILY, "" },
  { prefs::kWebKitSansSerifFontFamily, IDS_SANS_SERIF_FONT_FAMILY, "" },
  { prefs::kWebKitCursiveFontFamily, IDS_CURSIVE_FONT_FAMILY, "" },
  { prefs::kWebKitFantasyFontFamily, IDS_FANTASY_FONT_FAMILY, "" },
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_WIN)
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
    IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
#endif
#if defined(OS_CHROMEOS)
  { prefs::kWebKitStandardFontFamilyArabic, IDS_STANDARD_FONT_FAMILY_ARABIC,
    "ar" },
  { prefs::kWebKitSerifFontFamilyArabic, IDS_SERIF_FONT_FAMILY_ARABIC, "ar" },
  { prefs::kWebKitSansSerifFontFamilyArabic,
    IDS_SANS_SERIF_FONT_FAMILY_ARABIC, "ar" },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
#elif defined(OS_WIN)
  { prefs::kWebKitStandardFontFamilyCyrillic,
    IDS_STANDARD_FONT_FAMILY_CYRILLIC, kCyrllicLocales },
  { prefs::kWebKitFixedFontFamilyCyrillic,
    IDS_FIXED_FONT_FAMILY_CYRILLIC, kCyrllicLocales },
  { prefs::kWebKitSerifFontFamilyCyrillic,
    IDS_SERIF_FONT_FAMILY_CYRILLIC, kCyrllicLocales },
  { prefs::kWebKitSansSerifFontFamilyCyrillic,
    IDS_SANS_SERIF_FONT_FAMILY_CYRILLIC, kCyrllicLocales },
  { prefs::kWebKitStandardFontFamilyGreek,
    IDS_STANDARD_FONT_FAMILY_GREEK, "el" },
  { prefs::kWebKitFixedFontFamilyGreek, IDS_FIXED_FONT_FAMILY_GREEK, "el" },
  { prefs::kWebKitSerifFontFamilyGreek, IDS_SERIF_FONT_FAMILY_GREEK, "el" },
  { prefs::kWebKitSansSerifFontFamilyGreek,
    IDS_SANS_SERIF_FONT_FAMILY_GREEK, "el" },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN, "ko" },
  { prefs::kWebKitCursiveFontFamilyKorean, IDS_CURSIVE_FONT_FAMILY_KOREAN,
    "ko" },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN, "zh-CN" },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN, "zh-TW" },
#endif
};

const size_t kFontDefaultsLength = arraysize(kFontDefaults);

// Returns true if |locale| matches a string in comma-separated list
// |locale_list|.
static bool MatchesLocale(const std::string& locale,
                          const std::string& locale_list) {
  std::vector<std::string> list;
  base::SplitString(locale_list, ',', &list);
  for (std::vector<std::string>::const_iterator iter = list.begin();
       iter != list.end(); ++iter) {
    if (StartsWithASCII(locale, *iter, false))
      return true;
  }
  return false;
}

const struct {
  const char* from;
  const char* to;
} kPrefNamesToMigrate[] = {
  // Migrate prefs like "webkit.webprefs.standard_font_family" to
  // "webkit.webprefs.fonts.standard.Zyyy". This moves the formerly
  // "non-per-script" font prefs into the per-script font pref maps, as the
  // entry for the "Common" script (Zyyy is the ISO 15924 script code for the
  // Common script). The |from| prefs will exist if the migration to global
  // prefs (for the per-tab pref mechanism, which has since been removed) never
  // occurred.
  { prefs::kWebKitOldCursiveFontFamily,
    prefs::kWebKitCursiveFontFamily },
  { prefs::kWebKitOldFantasyFontFamily,
    prefs::kWebKitFantasyFontFamily },
  { prefs::kWebKitOldFixedFontFamily,
    prefs::kWebKitFixedFontFamily },
  { prefs::kWebKitOldSansSerifFontFamily,
    prefs::kWebKitSansSerifFontFamily },
  { prefs::kWebKitOldSerifFontFamily,
    prefs::kWebKitSerifFontFamily },
  { prefs::kWebKitOldStandardFontFamily,
    prefs::kWebKitStandardFontFamily },

  // Migrate "global" prefs. These will exist if the migration to global prefs
  // (for the per-tab pref mechanism, which has since been removed) occurred.
  // In addition, this moves the formerly "non-per-script" font prefs into the
  // per-script font pref maps, as above.
  { prefs::kGlobalDefaultCharset,
    prefs::kDefaultCharset },
  { prefs::kWebKitGlobalDefaultFixedFontSize,
    prefs::kWebKitDefaultFixedFontSize },
  { prefs::kWebKitGlobalDefaultFontSize,
    prefs::kWebKitDefaultFontSize },
  { prefs::kWebKitGlobalMinimumFontSize,
    prefs::kWebKitMinimumFontSize },
  { prefs::kWebKitGlobalMinimumLogicalFontSize,
    prefs::kWebKitMinimumLogicalFontSize },
  { prefs::kWebKitGlobalCursiveFontFamily,
    prefs::kWebKitCursiveFontFamily },
  { prefs::kWebKitGlobalFantasyFontFamily,
    prefs::kWebKitFantasyFontFamily },
  { prefs::kWebKitGlobalFixedFontFamily,
    prefs::kWebKitFixedFontFamily },
  { prefs::kWebKitGlobalSansSerifFontFamily,
    prefs::kWebKitSansSerifFontFamily },
  { prefs::kWebKitGlobalSerifFontFamily,
    prefs::kWebKitSerifFontFamily },
  { prefs::kWebKitGlobalStandardFontFamily,
    prefs::kWebKitStandardFontFamily }
};

const int kPrefsToMigrateLength = ARRAYSIZE_UNSAFE(kPrefNamesToMigrate);

static void MigratePreferences(PrefService* prefs) {
  RegisterPrefsToMigrate(prefs);
  for (int i = 0; i < kPrefsToMigrateLength; ++i) {
    const PrefService::Preference *pref =
        prefs->FindPreference(kPrefNamesToMigrate[i].from);
    if (!pref) continue;
    if (!pref->IsDefaultValue()) {
      prefs->Set(kPrefNamesToMigrate[i].to, *pref->GetValue()->DeepCopy());
    }
    prefs->ClearPref(kPrefNamesToMigrate[i].from);
    prefs->UnregisterPreference(kPrefNamesToMigrate[i].from);
  }
}

}  // namespace

PrefsTabHelper::PrefsTabHelper(WebContents* contents)
    : web_contents_(contents) {
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

  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents_->GetMutableRendererPrefs(), GetProfile());

  registrar_.Add(this, chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED,
                 content::NotificationService::AllSources());
#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(ENABLE_THEMES)
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
void PrefsTabHelper::RegisterUserPrefs(PrefService* prefs) {
  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled,
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
  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);

  // Register font prefs that have defaults.
  std::string locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < kFontDefaultsLength; ++i) {
    const FontDefault& pref = kFontDefaults[i];
    // Suppress default per-script font when the script matches the browser's
    // locale.  Otherwise, the default would override the user's preferences
    // when viewing pages in their native language.  This would be bad
    // particularly because there is not yet a way for users to customize
    // their per-script font prefs.  This code can possibly be removed later if
    // users can easily access per-script font prefs (e.g., via the extensions
    // workflow), or the problem turns out to not be really critical after all.
    if (!MatchesLocale(locale, pref.native_locale)) {
      prefs->RegisterLocalizedStringPref(pref.pref_name,
                                         pref.resource_id,
                                         PrefService::UNSYNCABLE_PREF);
    }
  }

  // Register font prefs that don't have defaults.
  RegisterFontFamilyMap(prefs, prefs::kWebKitStandardFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitFixedFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitSerifFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitSansSerifFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitCursiveFontFamilyMap);
  RegisterFontFamilyMap(prefs, prefs::kWebKitFantasyFontFamilyMap);

  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(
      prefs::kWebKitMinimumLogicalFontSize,
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

void PrefsTabHelper::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED:
      UpdateWebPreferences();
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      UpdateRendererPreferences();
      break;
    }
#endif
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = content::Details<std::string>(details).ptr();
      DCHECK(content::Source<PrefService>(source).ptr() ==
                 GetProfile()->GetPrefs());
      if (*pref_name_in == prefs::kDefaultCharset ||
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
  web_contents_->GetRenderViewHost()->UpdateWebkitPreferences(
      web_contents_->GetRenderViewHost()->GetWebkitPreferences());
}

void PrefsTabHelper::UpdateRendererPreferences() {
  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents_->GetMutableRendererPrefs(), GetProfile());
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

Profile* PrefsTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}
