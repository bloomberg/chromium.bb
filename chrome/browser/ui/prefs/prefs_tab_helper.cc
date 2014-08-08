// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/prefs/prefs_tab_helper.h"

#include <string>

#include "base/prefs/overlay_user_pref_store.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(ENABLE_THEMES)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::WebContents;
using content::WebPreferences;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PrefsTabHelper);

namespace {

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kDefaultCharset,
  prefs::kDisable3DAPIs,
  prefs::kEnableHyperlinkAuditing,
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitDomPasteEnabled,
#if defined(OS_ANDROID)
  prefs::kWebKitFontScaleFactor,
  prefs::kWebKitForceEnableZoom,
  prefs::kWebKitPasswordEchoEnabled,
#endif
  prefs::kWebKitInspectorSettings,
  prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitShrinksStandaloneImagesToFit,
  prefs::kWebkitTabsToLinks,
  prefs::kWebKitTextAreasAreResizable,
  prefs::kWebKitUsesUniversalDetector,
  prefs::kWebKitWebSecurityEnabled,
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

#if !defined(OS_ANDROID)
// Registers a preference under the path |pref_name| for each script used for
// per-script font prefs.
// For example, for WEBKIT_WEBPREFS_FONTS_SERIF ("fonts.serif"):
// "fonts.serif.Arab", "fonts.serif.Hang", etc. are registered.
// |fonts_with_defaults| contains all |pref_names| already registered since they
// have a specified default value.
// On Android there are no default values for these properties and there is no
// way to set them (because extensions are not supported so the Font Settings
// API cannot be used), so we can avoid registering them altogether.
void RegisterFontFamilyPrefs(user_prefs::PrefRegistrySyncable* registry,
                             const std::set<std::string>& fonts_with_defaults) {

  // Expand the font concatenated with script name so this stays at RO memory
  // rather than allocated in heap.
  static const char* const kFontFamilyMap[] = {
#define EXPAND_SCRIPT_FONT(map_name, script_name) map_name "." script_name,

#include "chrome/common/pref_font_script_names-inl.h"
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_CURSIVE)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_FANTASY)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_FIXED)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_PICTOGRAPH)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_SANSERIF)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_SERIF)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_STANDARD)

#undef EXPAND_SCRIPT_FONT
  };

  for (size_t i = 0; i < arraysize(kFontFamilyMap); ++i) {
    const char* pref_name = kFontFamilyMap[i];
    if (fonts_with_defaults.find(pref_name) == fonts_with_defaults.end()) {
      // We haven't already set a default value for this font preference, so set
      // an empty string as the default.
      registry->RegisterStringPref(
          pref_name,
          std::string(),
          user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
    }
  }
}
#endif  // !defined(OS_ANDROID)

// Registers |obs| to observe per-script font prefs under the path |map_name|.
// On android, there's no exposed way to change these prefs, so we can save
// ~715KB of heap and some startup cycles by avoiding observing these prefs
// since they will never change.
void RegisterFontFamilyMapObserver(
    PrefChangeRegistrar* registrar,
    const char* map_name,
    const PrefChangeRegistrar::NamedChangeCallback& obs) {
  DCHECK(StartsWithASCII(map_name, "webkit.webprefs.", true));
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    registrar->Add(pref_name.c_str(), obs);
  }
}

#if defined(OS_WIN)
// On Windows with antialising we want to use an alternate fixed font like
// Consolas, which looks much better than Courier New.
bool ShouldUseAlternateDefaultFixedFont(const std::string& script) {
  if (!StartsWithASCII(script, "courier", false))
    return false;
  UINT smooth_type = 0;
  SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smooth_type, 0);
  return (base::win::GetVersion() >= base::win::VERSION_WIN7) &&
         (smooth_type == FE_FONTSMOOTHINGCLEARTYPE);
}
#endif

struct FontDefault {
  const char* pref_name;
  int resource_id;
};

// Font pref defaults.  The prefs that have defaults vary by platform, since not
// all platforms have fonts for all scripts for all generic families.
// TODO(falken): add proper defaults when possible for all
// platforms/scripts/generic families.
const FontDefault kFontDefaults[] = {
  { prefs::kWebKitStandardFontFamily, IDS_STANDARD_FONT_FAMILY },
  { prefs::kWebKitFixedFontFamily, IDS_FIXED_FONT_FAMILY },
  { prefs::kWebKitSerifFontFamily, IDS_SERIF_FONT_FAMILY },
  { prefs::kWebKitSansSerifFontFamily, IDS_SANS_SERIF_FONT_FAMILY },
  { prefs::kWebKitCursiveFontFamily, IDS_CURSIVE_FONT_FAMILY },
  { prefs::kWebKitFantasyFontFamily, IDS_FANTASY_FONT_FAMILY },
  { prefs::kWebKitPictographFontFamily, IDS_PICTOGRAPH_FONT_FAMILY },
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_WIN)
  { prefs::kWebKitStandardFontFamilyJapanese,
    IDS_STANDARD_FONT_FAMILY_JAPANESE },
  { prefs::kWebKitFixedFontFamilyJapanese, IDS_FIXED_FONT_FAMILY_JAPANESE },
  { prefs::kWebKitSerifFontFamilyJapanese, IDS_SERIF_FONT_FAMILY_JAPANESE },
  { prefs::kWebKitSansSerifFontFamilyJapanese,
    IDS_SANS_SERIF_FONT_FAMILY_JAPANESE },
  { prefs::kWebKitStandardFontFamilyKorean, IDS_STANDARD_FONT_FAMILY_KOREAN },
  { prefs::kWebKitSerifFontFamilyKorean, IDS_SERIF_FONT_FAMILY_KOREAN },
  { prefs::kWebKitSansSerifFontFamilyKorean,
    IDS_SANS_SERIF_FONT_FAMILY_KOREAN },
  { prefs::kWebKitStandardFontFamilySimplifiedHan,
    IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN },
  { prefs::kWebKitSerifFontFamilySimplifiedHan,
    IDS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN },
  { prefs::kWebKitSansSerifFontFamilySimplifiedHan,
    IDS_SANS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN },
  { prefs::kWebKitStandardFontFamilyTraditionalHan,
    IDS_STANDARD_FONT_FAMILY_TRADITIONAL_HAN },
  { prefs::kWebKitSerifFontFamilyTraditionalHan,
    IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN },
  { prefs::kWebKitSansSerifFontFamilyTraditionalHan,
    IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN },
#endif
#if defined(OS_CHROMEOS)
  { prefs::kWebKitStandardFontFamilyArabic, IDS_STANDARD_FONT_FAMILY_ARABIC },
  { prefs::kWebKitSerifFontFamilyArabic, IDS_SERIF_FONT_FAMILY_ARABIC },
  { prefs::kWebKitSansSerifFontFamilyArabic,
    IDS_SANS_SERIF_FONT_FAMILY_ARABIC },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN },
#elif defined(OS_WIN)
  { prefs::kWebKitStandardFontFamilyCyrillic,
    IDS_STANDARD_FONT_FAMILY_CYRILLIC },
  { prefs::kWebKitFixedFontFamilyCyrillic, IDS_FIXED_FONT_FAMILY_CYRILLIC },
  { prefs::kWebKitSerifFontFamilyCyrillic, IDS_SERIF_FONT_FAMILY_CYRILLIC },
  { prefs::kWebKitSansSerifFontFamilyCyrillic,
    IDS_SANS_SERIF_FONT_FAMILY_CYRILLIC },
  { prefs::kWebKitStandardFontFamilyGreek, IDS_STANDARD_FONT_FAMILY_GREEK },
  { prefs::kWebKitFixedFontFamilyGreek, IDS_FIXED_FONT_FAMILY_GREEK },
  { prefs::kWebKitSerifFontFamilyGreek, IDS_SERIF_FONT_FAMILY_GREEK },
  { prefs::kWebKitSansSerifFontFamilyGreek, IDS_SANS_SERIF_FONT_FAMILY_GREEK },
  { prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN },
  { prefs::kWebKitCursiveFontFamilyKorean, IDS_CURSIVE_FONT_FAMILY_KOREAN },
  { prefs::kWebKitFixedFontFamilySimplifiedHan,
    IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN },
  { prefs::kWebKitFixedFontFamilyTraditionalHan,
    IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN },
#endif
};

const size_t kFontDefaultsLength = arraysize(kFontDefaults);

// Returns the script of the font pref |pref_name|.  For example, suppose
// |pref_name| is "webkit.webprefs.fonts.serif.Hant".  Since the script code for
// the script name "Hant" is USCRIPT_TRADITIONAL_HAN, the function returns
// USCRIPT_TRADITIONAL_HAN.  |pref_name| must be a valid font pref name.
UScriptCode GetScriptOfFontPref(const char* pref_name) {
  // ICU script names are four letters.
  static const size_t kScriptNameLength = 4;

  size_t len = strlen(pref_name);
  DCHECK_GT(len, kScriptNameLength);
  const char* scriptName = &pref_name[len - kScriptNameLength];
  int32 code = u_getPropertyValueEnum(UCHAR_SCRIPT, scriptName);
  DCHECK(code >= 0 && code < USCRIPT_CODE_LIMIT);
  return static_cast<UScriptCode>(code);
}

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used in font pref names.  For example,
// USCRIPT_HANGUL and USCRIPT_KOREAN are considered equivalent for the purposes
// of font selection.  Chrome uses the script code USCRIPT_HANGUL (script name
// "Hang") in Korean font pref names (for example,
// "webkit.webprefs.fonts.serif.Hang").  So, if |scriptCode| is USCRIPT_KOREAN,
// the function returns USCRIPT_HANGUL.  If |scriptCode| is not a member of such
// a family, returns |scriptCode|.
UScriptCode GetScriptForFontPrefMatching(UScriptCode scriptCode) {
  switch (scriptCode) {
  case USCRIPT_HIRAGANA:
  case USCRIPT_KATAKANA:
  case USCRIPT_KATAKANA_OR_HIRAGANA:
    return USCRIPT_JAPANESE;
  case USCRIPT_KOREAN:
    return USCRIPT_HANGUL;
  default:
    return scriptCode;
  }
}

// Returns the primary script used by the browser's UI locale.  For example, if
// the locale is "ru", the function returns USCRIPT_CYRILLIC, and if the locale
// is "en", the function returns USCRIPT_LATIN.
UScriptCode GetScriptOfBrowserLocale() {
  std::string locale = g_browser_process->GetApplicationLocale();

  // For Chinese locales, uscript_getCode() just returns USCRIPT_HAN but our
  // per-script fonts are for USCRIPT_SIMPLIFIED_HAN and
  // USCRIPT_TRADITIONAL_HAN.
  if (locale == "zh-CN")
    return USCRIPT_SIMPLIFIED_HAN;
  if (locale == "zh-TW")
    return USCRIPT_TRADITIONAL_HAN;

  UScriptCode code = USCRIPT_INVALID_CODE;
  UErrorCode err = U_ZERO_ERROR;
  uscript_getCode(locale.c_str(), &code, 1, &err);

  // Ignore the error that multiple scripts could be returned, since we only
  // want one script.
  if (U_FAILURE(err) && err != U_BUFFER_OVERFLOW_ERROR)
    code = USCRIPT_INVALID_CODE;
  return GetScriptForFontPrefMatching(code);
}

// Sets a font family pref in |prefs| to |pref_value|.
void OverrideFontFamily(WebPreferences* prefs,
                        const std::string& generic_family,
                        const std::string& script,
                        const std::string& pref_value) {
  content::ScriptFontFamilyMap* map = NULL;
  if (generic_family == "standard")
    map = &prefs->standard_font_family_map;
  else if (generic_family == "fixed")
    map = &prefs->fixed_font_family_map;
  else if (generic_family == "serif")
    map = &prefs->serif_font_family_map;
  else if (generic_family == "sansserif")
    map = &prefs->sans_serif_font_family_map;
  else if (generic_family == "cursive")
    map = &prefs->cursive_font_family_map;
  else if (generic_family == "fantasy")
    map = &prefs->fantasy_font_family_map;
  else if (generic_family == "pictograph")
    map = &prefs->pictograph_font_family_map;
  else
    NOTREACHED() << "Unknown generic font family: " << generic_family;
  (*map)[script] = base::UTF8ToUTF16(pref_value);
}

}  // namespace

PrefsTabHelper::PrefsTabHelper(WebContents* contents)
    : web_contents_(contents),
      weak_ptr_factory_(this) {
  PrefService* prefs = GetProfile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    base::Closure renderer_callback = base::Bind(
        &PrefsTabHelper::UpdateRendererPreferences, base::Unretained(this));
    pref_change_registrar_.Add(prefs::kAcceptLanguages, renderer_callback);
    pref_change_registrar_.Add(prefs::kDefaultZoomLevel, renderer_callback);
    pref_change_registrar_.Add(prefs::kEnableDoNotTrack, renderer_callback);
    pref_change_registrar_.Add(prefs::kEnableReferrers, renderer_callback);

#if !defined(OS_MACOSX)
    pref_change_registrar_.Add(prefs::kFullscreenAllowed, renderer_callback);
#endif

    PrefChangeRegistrar::NamedChangeCallback webkit_callback = base::Bind(
        &PrefsTabHelper::OnWebPrefChanged, base::Unretained(this));
    for (int i = 0; i < kPrefsToObserveLength; ++i) {
      const char* pref_name = kPrefsToObserve[i];
      pref_change_registrar_.Add(pref_name, webkit_callback);
    }

    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitStandardFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitFixedFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitSerifFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitSansSerifFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitCursiveFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitFantasyFontFamilyMap,
                                  webkit_callback);
    RegisterFontFamilyMapObserver(&pref_change_registrar_,
                                  prefs::kWebKitPictographFontFamilyMap,
                                  webkit_callback);
  }

  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents_->GetMutableRendererPrefs(), GetProfile());

#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(ENABLE_THEMES)
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
#endif
#if defined(USE_AURA)
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_FLING_CURVE_PARAMETERS_CHANGED,
                 content::NotificationService::AllSources());
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
  pref_store->RegisterOverlayPref(prefs::kSaveFileDefaultDirectory);
#if defined(OS_ANDROID) || defined(OS_IOS)
  pref_store->RegisterOverlayPref(prefs::kProxy);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
}

// static
void PrefsTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  WebPreferences pref_defaults;
  registry->RegisterBooleanPref(
      prefs::kWebKitJavascriptEnabled,
      pref_defaults.javascript_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitWebSecurityEnabled,
      pref_defaults.web_security_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitLoadsImagesAutomatically,
      pref_defaults.loads_images_automatically,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitPluginsEnabled,
      pref_defaults.plugins_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitDomPasteEnabled,
      pref_defaults.dom_paste_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitShrinksStandaloneImagesToFit,
      pref_defaults.shrinks_standalone_images_to_fit,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kWebKitInspectorSettings,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitTextAreasAreResizable,
      pref_defaults.text_areas_are_resizable,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitJavaEnabled,
      pref_defaults.java_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebkitTabsToLinks,
      pref_defaults.tabs_to_links,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitAllowRunningInsecureContent,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitAllowDisplayingInsecureContent,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableReferrers,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterDoublePref(
      prefs::kWebKitFontScaleFactor,
      1.0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitForceEnableZoom,
      pref_defaults.force_enable_zoom,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebKitPasswordEchoEnabled,
      pref_defaults.password_echo_enabled,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterLocalizedStringPref(
      prefs::kAcceptLanguages,
      IDS_ACCEPT_LANGUAGES,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterLocalizedStringPref(
      prefs::kDefaultCharset,
      IDS_DEFAULT_ENCODING,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Register font prefs that have defaults.
  std::set<std::string> fonts_with_defaults;
  UScriptCode browser_script = GetScriptOfBrowserLocale();
  for (size_t i = 0; i < kFontDefaultsLength; ++i) {
    FontDefault pref = kFontDefaults[i];

#if defined(OS_WIN)
    if (pref.pref_name == prefs::kWebKitFixedFontFamily) {
      if (ShouldUseAlternateDefaultFixedFont(
              l10n_util::GetStringUTF8(pref.resource_id)))
        pref.resource_id = IDS_FIXED_FONT_FAMILY_ALT_WIN;
    }
#endif

    UScriptCode pref_script = GetScriptOfFontPref(pref.pref_name);

    // Suppress this default font pref value if it is for the primary script of
    // the browser's UI locale.  For example, if the pref is for the sans-serif
    // font for the Cyrillic script, and the browser locale is "ru" (Russian),
    // the default is suppressed.  Otherwise, the default would override the
    // user's font preferences when viewing pages in their native language.
    // This is because users have no way yet of customizing their per-script
    // font preferences.  The font prefs accessible in the options UI are for
    // the default, unknown script; these prefs have less priority than the
    // per-script font prefs when the script of the content is known.  This code
    // can possibly be removed later if users can easily access per-script font
    // prefs (e.g., via the extensions workflow), or the problem turns out to
    // not be really critical after all.
    if (browser_script != pref_script) {
      registry->RegisterLocalizedStringPref(
          pref.pref_name,
          pref.resource_id,
          user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
      fonts_with_defaults.insert(pref.pref_name);
    }
  }

  // Register per-script font prefs that don't have defaults.
#if !defined(OS_ANDROID)
  RegisterFontFamilyPrefs(registry, fonts_with_defaults);
#endif

  registry->RegisterLocalizedIntegerPref(
      prefs::kWebKitDefaultFontSize,
      IDS_DEFAULT_FONT_SIZE,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterLocalizedIntegerPref(
      prefs::kWebKitDefaultFixedFontSize,
      IDS_DEFAULT_FIXED_FONT_SIZE,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterLocalizedIntegerPref(
      prefs::kWebKitMinimumFontSize,
      IDS_MINIMUM_FONT_SIZE,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterLocalizedIntegerPref(
      prefs::kWebKitMinimumLogicalFontSize,
      IDS_MINIMUM_LOGICAL_FONT_SIZE,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterLocalizedBooleanPref(
      prefs::kWebKitUsesUniversalDetector,
      IDS_USES_UNIVERSAL_DETECTOR,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterLocalizedStringPref(
      prefs::kStaticEncodings,
      IDS_STATIC_ENCODING_LIST,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kRecentlySelectedEncoding,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void PrefsTabHelper::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(ENABLE_THEMES)
  if (type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED) {
    UpdateRendererPreferences();
    return;
  }
#endif

#if defined(USE_AURA)
  if (type == chrome::NOTIFICATION_BROWSER_FLING_CURVE_PARAMETERS_CHANGED) {
    UpdateRendererPreferences();
    return;
  }
#endif  // defined(USE_AURA)

  NOTREACHED();
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

void PrefsTabHelper::OnFontFamilyPrefChanged(const std::string& pref_name) {
  // When a font family pref's value goes from non-empty to the empty string, we
  // must add it to the usual WebPreferences struct passed to the renderer.
  //
  // The empty string means to fall back to the pref for the Common script
  // ("Zyyy").  For example, if chrome.fonts.serif.Cyrl is the empty string, it
  // means to use chrome.fonts.serif.Zyyy for Cyrillic script. Prefs that are
  // the empty string are normally not passed to WebKit, since there are so many
  // of them that it would cause a performance regression. Not passing the pref
  // is normally okay since WebKit does the desired fallback behavior regardless
  // of whether the empty string is passed or the pref is not passed at all. But
  // if the pref has changed from non-empty to the empty string, we must let
  // WebKit know.
  std::string generic_family;
  std::string script;
  if (pref_names_util::ParseFontNamePrefPath(pref_name,
                                             &generic_family,
                                             &script)) {
    PrefService* prefs = GetProfile()->GetPrefs();
    std::string pref_value = prefs->GetString(pref_name.c_str());
    if (pref_value.empty()) {
      WebPreferences web_prefs =
          web_contents_->GetRenderViewHost()->GetWebkitPreferences();
      OverrideFontFamily(&web_prefs, generic_family, script, std::string());
      web_contents_->GetRenderViewHost()->UpdateWebkitPreferences(web_prefs);
      return;
    }
  }
}

void PrefsTabHelper::OnWebPrefChanged(const std::string& pref_name) {
#if !defined(OS_ANDROID)
  OnFontFamilyPrefChanged(pref_name);
#endif

  web_contents_->GetRenderViewHost()->OnWebkitPreferencesChanged();
}
