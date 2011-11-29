// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/utf_string_conversions.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter_observer.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/pdf_unsupported_feature.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory_impl.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/sad_tab_observer.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/per_tab_prefs_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webpreferences.h"

namespace {

static base::LazyInstance<base::PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor = LAZY_INSTANCE_INITIALIZER;

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kDefaultCharset,
  prefs::kDefaultZoomLevel,
  prefs::kEnableReferrers,
#if defined (ENABLE_SAFE_BROWSING)
  prefs::kSafeBrowsingEnabled,
#endif
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitCursiveFontFamily,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitFantasyFontFamily,
  prefs::kWebKitFixedFontFamily,
  prefs::kWebKitGlobalJavascriptEnabled,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitSansSerifFontFamily,
  prefs::kWebKitSerifFontFamily,
  prefs::kWebKitStandardFontFamily,
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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : TabContentsObserver(contents),
      delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          synced_tab_delegate_(new TabContentsWrapperSyncedTabDelegate(this))),
      in_destructor_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  DCHECK(!GetCurrentWrapperForContents(contents));
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Create the tab helpers.
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_manager_ = new AutofillManager(this);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExternalAutofillPopup)) {
    autofill_external_delegate_.reset(
        AutofillExternalDelegate::Create(this, autofill_manager_.get()));
    autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
    autocomplete_history_manager_->SetExternalDelegate(
        autofill_external_delegate_.get());
  }
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  constrained_window_tab_helper_.reset(new ConstrainedWindowTabHelper(this));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  infobar_tab_helper_.reset(new InfoBarTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  per_tab_prefs_tab_helper_.reset(new PerTabPrefsTabHelper(this));
  prerender_tab_helper_.reset(new prerender::PrerenderTabHelper(this));
  print_view_manager_.reset(new printing::PrintViewManager(this));
  restore_tab_helper_.reset(new RestoreTabHelper(this));
#if defined(ENABLE_SAFE_BROWSING)
  if (profile()->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_detection_service()) {
    safebrowsing_detection_host_.reset(
        safe_browsing::ClientSideDetectionHost::Create(contents));
  }
#endif
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  web_intent_picker_controller_.reset(new WebIntentPickerController(
      this, new WebIntentPickerFactoryImpl()));

  // Create the per-tab observers.
  download_request_limiter_observer_.reset(
      new DownloadRequestLimiterObserver(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  plugin_observer_.reset(new PluginObserver(this));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  sad_tab_observer_.reset(new SadTabObserver(contents));
  // Start the in-browser thumbnailing if the feature is enabled.
  if (switches::IsInBrowserThumbnailingEnabled()) {
    thumbnail_generation_observer_.reset(new ThumbnailGenerator);
    thumbnail_generation_observer_->StartThumbnailing(tab_contents_.get());
  }

  // Set-up the showing of the omnibox search infobar if applicable.
  if (OmniboxSearchHint::IsEnabled(profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED,
                 content::NotificationService::AllSources());
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile())));
#endif

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile()->GetPrefs();
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
      tab_contents()->GetMutableRendererPrefs(), profile());
}

TabContentsWrapper::~TabContentsWrapper() {
  in_destructor_ = true;

  // Need to tear down infobars before the TabContents goes away.
  infobar_tab_helper_.reset();
}

base::PropertyAccessor<TabContentsWrapper*>*
    TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

// static
void TabContentsWrapper::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitGlobalJavascriptEnabled,
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
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR,
                                      PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding,
                            "",
                            PrefService::UNSYNCABLE_PREF);
}

string16 TabContentsWrapper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 TabContentsWrapper::GetStatusText() const {
  if (!tab_contents()->IsLoading() ||
      tab_contents()->load_state().state == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (tab_contents()->load_state().state) {
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
                                        tab_contents()->load_state().param);
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(
          IDS_LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SSL_HANDSHAKE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
    case net::LOAD_STATE_SENDING_REQUEST:
      if (tab_contents()->upload_size())
        return l10n_util::GetStringFUTF16Int(
                    IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
                    static_cast<int>((100 * tab_contents()->upload_position()) /
                        tab_contents()->upload_size()));
      else
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        tab_contents()->load_state_host());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return string16();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  TabContents* new_contents = tab_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);

  new_wrapper->extension_tab_helper()->CopyStateFrom(
      *extension_tab_helper_.get());
  return new_wrapper;
}

void TabContentsWrapper::CaptureSnapshot() {
  Send(new ChromeViewMsg_CaptureSnapshot(routing_id()));
}

// static
TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

// static
const TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    const TabContents* contents) {
  TabContentsWrapper* const* wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

Profile* TabContentsWrapper::profile() const {
  return Profile::FromBrowserContext(tab_contents()->browser_context());
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper implementation:

void TabContentsWrapper::RenderViewCreated(RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

void TabContentsWrapper::DidBecomeSelected() {
  WebCacheManager::GetInstance()->ObserveActivity(
      tab_contents()->GetRenderProcessHost()->GetID());
}

bool TabContentsWrapper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsWrapper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Snapshot, OnSnapshot)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabContentsWrapper::TabContentsDestroyed(TabContents* tab) {
  // Destruction of the TabContents should only be done by us from our
  // destructor. Otherwise it's very likely we (or one of the helpers we own)
  // will attempt to access the TabContents and we'll crash.
  DCHECK(in_destructor_);
}

void TabContentsWrapper::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL(render_view_host());
      break;
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
             profile()->GetPrefs() ||
             content::Source<PrefService>(source).ptr() ==
             per_tab_prefs_tab_helper_->prefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL(render_view_host());
      } else if ((*pref_name_in == prefs::kDefaultCharset) ||
                 StartsWithASCII(*pref_name_in, "webkit.webprefs.", true)) {
        UpdateWebPreferences();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
        tab_contents()->render_view_host()->SetZoomLevel(
            tab_contents()->GetZoomLevel());
      } else if (*pref_name_in == prefs::kEnableReferrers) {
        UpdateRendererPreferences();
      } else if (*pref_name_in == prefs::kSafeBrowsingEnabled) {
        UpdateSafebrowsingDetectionHost();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::OnSnapshot(const SkBitmap& bitmap) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
      content::Source<TabContentsWrapper>(this),
      content::Details<const SkBitmap>(&bitmap));
}

void TabContentsWrapper::OnPDFHasUnsupportedFeature() {
  PDFHasUnsupportedFeature(this);
}

GURL TabContentsWrapper::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in Incognito mode.
  if (profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = profile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

void TabContentsWrapper::UpdateAlternateErrorPageURL(RenderViewHost* rvh) {
  rvh->SetAltErrorPageURL(GetAlternateErrorPageURL());
}

void TabContentsWrapper::UpdateWebPreferences() {
  RenderViewHostDelegate* rvhd = tab_contents();
  WebPreferences prefs = rvhd->GetWebkitPrefs();
  per_tab_prefs_tab_helper_->OverrideWebPreferences(&prefs);
  tab_contents()->render_view_host()->UpdateWebkitPreferences(prefs);
}

void TabContentsWrapper::UpdateRendererPreferences() {
  renderer_preferences_util::UpdateFromSystemSettings(
      tab_contents()->GetMutableRendererPrefs(), profile());
  render_view_host()->SyncRendererPrefs();
}

void TabContentsWrapper::UpdateSafebrowsingDetectionHost() {
#if defined(ENABLE_SAFE_BROWSING)
  PrefService* prefs = profile()->GetPrefs();
  bool safe_browsing = prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  if (safe_browsing &&
      g_browser_process->safe_browsing_detection_service()) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_.reset(
          safe_browsing::ClientSideDetectionHost::Create(tab_contents()));
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
  render_view_host()->Send(
      new ChromeViewMsg_SetClientSidePhishingDetection(routing_id(),
                                                       safe_browsing));
#endif
}

void TabContentsWrapper::ExitFullscreenMode() {
  if (tab_contents() && render_view_host())
    tab_contents()->render_view_host()->ExitFullscreen();
}
