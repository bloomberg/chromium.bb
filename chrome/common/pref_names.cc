// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// A boolean specifying whether the New Tab page is the home page or not.
const wchar_t kHomePageIsNewTabPage[] = L"homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const wchar_t kHomePage[] = L"homepage";

// Used to determine if the last session exited cleanly. Set to false when
// first opened, and to true when closing. On startup if the value is false,
// it means the profile didn't exit cleanly.
const wchar_t kSessionExitedCleanly[] = L"profile.exited_cleanly";

// An integer pref. Holds one of several values:
// 0: (or empty) don't do anything special on startup.
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
const wchar_t kRestoreOnStartup[] = L"session.restore_on_startup";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
const wchar_t kURLsToRestoreOnStartup[] =
    L"session.urls_to_restore_on_startup";

// The application locale.
const wchar_t kApplicationLocale[] = L"intl.app_locale";

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
const wchar_t kDefaultCharset[] = L"intl.charset_default";

// The value to use for Accept-Languages HTTP header when making an HTTP
// request.
const wchar_t kAcceptLanguages[] = L"intl.accept_languages";

// The value to use for showing locale-dependent encoding list for different
// locale, it's initialized from the corresponding string resource that is
// stored in non-translatable part of the resource bundle.
const wchar_t kStaticEncodings[] = L"intl.static_encodings";

// OBSOLETE.  The list of hostnames for which we whitelist popups (rather than
// blocking).
const wchar_t kPopupWhitelistedHosts[] = L"profile.popup_whitelisted_sites";

// WebKit preferences.
// A boolean flag to indicate whether WebKit standard font family is
// serif or sans-serif. We don't have a UI for setting standard family.
// Instead, we use this pref to map either serif or sans_serif to WebKit
// standard font family. At the moment, we don't have a UI for this
// flag, either.
const wchar_t kWebKitStandardFontIsSerif[] =
    L"webkit.webprefs.standard_font_is_serif";
const wchar_t kWebKitFixedFontFamily[] = L"webkit.webprefs.fixed_font_family";
const wchar_t kWebKitSerifFontFamily[] = L"webkit.webprefs.serif_font_family";
const wchar_t kWebKitSansSerifFontFamily[] =
    L"webkit.webprefs.sansserif_font_family";
const wchar_t kWebKitCursiveFontFamily[] =
    L"webkit.webprefs.cursive_font_family";
const wchar_t kWebKitFantasyFontFamily[] =
    L"webkit.webprefs.fantasy_font_family";
const wchar_t kWebKitDefaultFontSize[] = L"webkit.webprefs.default_font_size";
const wchar_t kWebKitDefaultFixedFontSize[] =
    L"webkit.webprefs.default_fixed_font_size";
const wchar_t kWebKitMinimumFontSize[] = L"webkit.webprefs.minimum_font_size";
const wchar_t kWebKitMinimumLogicalFontSize[] =
    L"webkit.webprefs.minimum_logical_font_size";
const wchar_t kWebKitJavascriptEnabled[] =
    L"webkit.webprefs.javascript_enabled";
const wchar_t kWebKitWebSecurityEnabled[] =
    L"webkit.webprefs.web_security_enabled";
const wchar_t kWebKitJavascriptCanOpenWindowsAutomatically[] =
    L"webkit.webprefs.javascript_can_open_windows_automatically";
const wchar_t kWebKitLoadsImagesAutomatically[] =
    L"webkit.webprefs.loads_images_automatically";
const wchar_t kWebKitPluginsEnabled[] = L"webkit.webprefs.plugins_enabled";
const wchar_t kWebKitDomPasteEnabled[] = L"webkit.webprefs.dom_paste_enabled";
const wchar_t kWebKitShrinksStandaloneImagesToFit[] =
    L"webkit.webprefs.shrinks_standalone_images_to_fit";
const wchar_t kWebKitInspectorSettings[] =
    L"webkit.webprefs.inspector_settings";
const wchar_t kWebKitUsesUniversalDetector[] =
    L"webkit.webprefs.uses_universal_detector";
const wchar_t kWebKitTextAreasAreResizable[] =
    L"webkit.webprefs.text_areas_are_resizable";
const wchar_t kWebKitJavaEnabled[] =
    L"webkit.webprefs.java_enabled";
const wchar_t kWebkitTabsToLinks[] = L"webkit.webprefs.tabs_to_links";

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const wchar_t kShowBookmarkBar[] = L"bookmark_bar.show_on_all_tabs";

// Boolean that is true if the password manager is on (will record new
// passwords and fill in known passwords).
const wchar_t kPasswordManagerEnabled[] = L"profile.password_manager_enabled";

// OBSOLETE.  Boolean that is true if the form AutoFill is on (will record
// values entered in text inputs in forms and shows them in a popup when user
// type in a text input with the same name later on).  This has been superseded
// by kAutoFillEnabled.
const wchar_t kFormAutofillEnabled[] = L"profile.form_autofill_enabled";

// Boolean that is true when SafeBrowsing is enabled.
const wchar_t kSafeBrowsingEnabled[] = L"safebrowsing.enabled";

// Boolean that is true when Suggest support is enabled.
const wchar_t kSearchSuggestEnabled[] = L"search.suggest_enabled";

// OBSOLETE.  Enum that specifies whether to enforce a third-party cookie
// blocking policy.  This has been superseded by kDefaultContentSettings +
// kBlockThirdPartyCookies.
// 0 - allow all cookies.
// 1 - block third-party cookies
// 2 - block all cookies
const wchar_t kCookieBehavior[] = L"security.cookie_behavior";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for searches.
const wchar_t kDefaultSearchProviderSearchURL[] =
    L"default_search_provider.search_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for suggestions.
const wchar_t kDefaultSearchProviderSuggestURL[] =
    L"default_search_provider.suggest_url";

// The name of the default search provider.
const wchar_t kDefaultSearchProviderName[] = L"default_search_provider.name";

// The id of the default search provider.
const wchar_t kDefaultSearchProviderID[] = L"default_search_provider.id";

// The prepopulate id of the default search provider.
const wchar_t kDefaultSearchProviderPrepopulateID[] =
    L"default_search_provider.prepopulate_id";

// The dictionary key used when the default search providers are given
// in the preferences file. Normally they are copied from the master
// preferences file.
const wchar_t kSearchProviderOverrides[] =
    L"search_provider_overrides";
// The format version for the dictionary above.
const wchar_t kSearchProviderOverridesVersion[] =
    L"search_provider_overrides_version";

// Boolean which specifies whether we should ask the user if we should download
// a file (true) or just download it automatically.
const wchar_t kPromptForDownload[] = L"download.prompt_for_download";

// A boolean pref set to true if we're using Link Doctor error pages.
const wchar_t kAlternateErrorPagesEnabled[] = L"alternate_error_pages.enabled";

// A boolean pref set to true if DNS pre-fetching is being done in browser.
const wchar_t kDnsPrefetchingEnabled[] = L"dns_prefetching.enabled";

// An adaptively identified list of domain names to be pre-fetched during the
// next startup, based on what was actually needed during this startup.
const wchar_t kDnsStartupPrefetchList[] = L"StartupDNSPrefetchList";

// A list of host names used to fetch web pages, and their commonly used
// sub-resource hostnames (and expected latency benefits from pre-resolving, or
// preconnecting to, such sub-resource hostnames).
// This list is adaptively grown and pruned.
const wchar_t kDnsHostReferralList[] = L"HostReferralList";

// Is the cookie prompt expanded?
const wchar_t kCookiePromptExpanded[] = L"cookieprompt.expanded";

#if defined(USE_NSS)
// Prefs for SSLConfigServicePref.  Currently, these are only present on
// and used by NSS-using OSes.
const wchar_t kCertRevocationCheckingEnabled[] = L"ssl.rev_checking.enabled";
const wchar_t kSSL2Enabled[] = L"ssl.ssl2.enabled";
const wchar_t kSSL3Enabled[] = L"ssl.ssl3.enabled";
const wchar_t kTLS1Enabled[] = L"ssl.tls1.enabled";
#endif

#if defined(OS_CHROMEOS)
// A boolean pref set to true if TapToClick is being done in browser.
const wchar_t kTapToClickEnabled[] = L"settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if VertEdgeScroll is being done in browser.
const wchar_t kVertEdgeScrollEnabled[] =
    L"settings.touchpad.enable_vert_edge_scroll";

// A integer pref for the touchpad speed factor.
const wchar_t kTouchpadSpeedFactor[] = L"settings.touchpad.speed_factor";

// A integer pref for the touchpad sensitivity.
const wchar_t kTouchpadSensitivity[] = L"settings.touchpad.sensitivity";

// A string pref set to the current input method.
const wchar_t kLanguageCurrentInputMethod[] =
    L"settings.language.current_input_method";

// A string pref set to the previous input method.
const wchar_t kLanguagePreviousInputMethod[] =
    L"settings.language.previous_input_method";

// A string pref (comma-separated list) set to the "next engine in menu"
// hot-key lists.
const wchar_t kLanguageHotkeyNextEngineInMenu[] =
    L"settings.language.hotkey_next_engine_in_menu";

// A string pref (comma-separated list) set to the "previous engine"
// hot-key lists.
const wchar_t kLanguageHotkeyPreviousEngine[] =
    L"settings.language.hotkey_previous_engine";

// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const wchar_t kLanguagePreferredLanguages[] =
    L"settings.language.preferred_languages";

// A string pref (comma-separated list) set to the preloaded (active) input
// method IDs (ex. "pinyin,mozc").
const wchar_t kLanguagePreloadEngines[] = L"settings.language.preload_engines";

// Boolean prefs for ibus-chewing Chinese input method.
const wchar_t kLanguageChewingAutoShiftCur[] =
    L"settings.language.chewing_auto_shift_cur";
const wchar_t kLanguageChewingAddPhraseDirection[] =
    L"settings.language.chewing_add_phrase_direction";
const wchar_t kLanguageChewingEasySymbolInput[] =
    L"settings.language.chewing_easy_symbol_input";
const wchar_t kLanguageChewingEscCleanAllBuf[] =
    L"settings.language.chewing_esc_clean_all_buf";
const wchar_t kLanguageChewingForceLowercaseEnglish[] =
    L"settings.language.chewing_force_lowercase_english";
const wchar_t kLanguageChewingPlainZhuyin[] =
    L"settings.language.chewing_plain_zhuyin";
const wchar_t kLanguageChewingPhraseChoiceRearward[] =
    L"settings.language.chewing_phrase_choice_rearward";
const wchar_t kLanguageChewingSpaceAsSelection[] =
    L"settings.language.chewing_space_as_selection";

// Integer prefs for ibus-chewing Chinese input method.
const wchar_t kLanguageChewingMaxChiSymbolLen[] =
    L"settings.language.chewing_max_chi_symbol_len";
const wchar_t kLanguageChewingCandPerPage[] =
    L"settings.language.chewing_cand_per_page";

// String prefs for ibus-chewing Chinese input method.
const wchar_t kLanguageChewingKeyboardType[] =
    L"settings.language.chewing_keyboard_type";
const wchar_t kLanguageChewingSelKeys[] =
    L"settings.language.chewing_sel_keys";

const wchar_t kLanguageChewingHsuSelKeyType[] =
    L"settings.language.chewing_hsu_sel_key_type";

// A string pref which determines the keyboard layout for Hangul input method.
const wchar_t kLanguageHangulKeyboard[] = L"settings.language.hangul_keyboard";
const wchar_t kLanguageHangulHanjaKeys[] =
    L"settings.language.hangul_hanja_keys";

// A boolean prefs for ibus-pinyin Chinese input method.
const wchar_t kLanguagePinyinCorrectPinyin[] =
    L"settings.language.pinyin_correct_pinyin";
const wchar_t kLanguagePinyinFuzzyPinyin[] =
    L"settings.language.pinyin_fuzzy_pinyin";
const wchar_t kLanguagePinyinShiftSelectCandidate[] =
    L"settings.language.pinyin_shift_select_candidate";
const wchar_t kLanguagePinyinMinusEqualPage[] =
    L"settings.language.pinyin_minus_equal_page";
const wchar_t kLanguagePinyinCommaPeriodPage[] =
    L"settings.language.pinyin_comma_period_page";
const wchar_t kLanguagePinyinAutoCommit[] =
    L"settings.language.pinyin_auto_commit";
const wchar_t kLanguagePinyinDoublePinyin[] =
    L"settings.language.pinyin_double_pinyin";
const wchar_t kLanguagePinyinInitChinese[] =
    L"settings.language.pinyin_init_chinese";
const wchar_t kLanguagePinyinInitFull[] =
    L"settings.language.pinyin_init_full";
const wchar_t kLanguagePinyinInitFullPunct[] =
    L"settings.language.pinyin_init_full_punct";
const wchar_t kLanguagePinyinInitSimplifiedChinese[] =
    L"settings.language.pinyin_init_simplified_chinese";
const wchar_t kLanguagePinyinTradCandidate[] =
    L"settings.language.pinyin_trad_candidate";

// A integer prefs for ibus-pinyin Chinese input method.
const wchar_t kLanguagePinyinDoublePinyinSchema[] =
    L"settings.language.pinyin_double_pinyin_schema";
const wchar_t kLanguagePinyinLookupTablePageSize[] =
    L"settings.language.pinyin_lookup_table_page_size";

// A string prefs for ibus-mozc Japanese input method.
// ibus-mozc converts the string values to protobuf enum values defined in
// third_party/ibus-mozc/files/src/session/config.proto.
const wchar_t kLanguageMozcPreeditMethod[] =
    L"settings.language.mozc_preedit_method";
const wchar_t kLanguageMozcSessionKeymap[] =
    L"settings.language.mozc_sessoin_keymap";
const wchar_t kLanguageMozcPunctuationMethod[] =
    L"settings.language.mozc_punctuation_method";
const wchar_t kLanguageMozcSymbolMethod[] =
    L"settings.language.mozc_symbol_method";
const wchar_t kLanguageMozcSpaceCharacterForm[] =
    L"settings.language.mozc_space_character_form";
const wchar_t kLanguageMozcHistoryLearningLevel[] =
    L"settings.language.mozc_history_learning_level";
const wchar_t kLanguageMozcSelectionShortcut[] =
    L"settings.language.mozc_selection_shortcut";
const wchar_t kLanguageMozcShiftKeyModeSwitch[] =
    L"settings.language.mozc_shift_key_mode_switch";
const wchar_t kLanguageMozcNumpadCharacterForm[] =
    L"settings.language.mozc_numpad_character_form";
const wchar_t kLanguageMozcIncognitoMode[] =
    L"settings.language.mozc_incognito_mode";
const wchar_t kLanguageMozcUseAutoImeTurnOff[] =
    L"settings.language.mozc_use_auto_ime_turn_off";
const wchar_t kLanguageMozcUseDateConversion[] =
    L"settings.language.mozc_use_date_conversion";
const wchar_t kLanguageMozcUseSingleKanjiConversion[] =
    L"settings.language.mozc_use_single_kanji_conversion";
const wchar_t kLanguageMozcUseSymbolConversion[] =
    L"settings.language.mozc_use_symbol_conversion";
const wchar_t kLanguageMozcUseNumberConversion[] =
    L"settings.language.mozc_use_number_conversion";
const wchar_t kLanguageMozcUseHistorySuggest[] =
    L"settings.language.mozc_use_history_suggest";
const wchar_t kLanguageMozcUseDictionarySuggest[] =
    L"settings.language.mozc_use_dictionary_suggest";
const wchar_t kLanguageMozcSuggestionsSize[] =
    L"settings.language.mozc_suggestions_size";

// A boolean pref which determines whether accessibility is enabled.
const wchar_t kAccessibilityEnabled[] = L"settings.accessibility";

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
const wchar_t kLabsAdvancedFilesystemEnabled[] =
    L"settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
const wchar_t kLabsMediaplayerEnabled[] = L"settings.labs.mediaplayer";

#endif  // defined(OS_CHROMEOS)

// The disabled messages in IPC logging.
const wchar_t kIpcDisabledMessages[] = L"ipc_log_disabled_messages";

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
const wchar_t kShowHomeButton[] = L"browser.show_home_button";

// A boolean pref set to true if the Page and Options menu buttons should be
// visible on the toolbar. This is only used for Mac where the default is to
// have these menu in the main menubar, not as buttons on the toolbar.
const wchar_t kShowPageOptionsButtons[] = L"browser.show_page_options_buttons";

// A string value which saves short list of recently user selected encodings
// separated with comma punctuation mark.
const wchar_t kRecentlySelectedEncoding[] =
    L"profile.recently_selected_encodings";

// Clear Browsing Data dialog preferences.
const wchar_t kDeleteBrowsingHistory[] = L"browser.clear_data.browsing_history";
const wchar_t kDeleteDownloadHistory[] =
    L"browser.clear_data.download_history";
const wchar_t kDeleteCache[] = L"browser.clear_data.cache";
const wchar_t kDeleteCookies[] = L"browser.clear_data.cookies";
const wchar_t kDeletePasswords[] = L"browser.clear_data.passwords";
const wchar_t kDeleteFormData[] = L"browser.clear_data.form_data";
const wchar_t kDeleteTimePeriod[] = L"browser.clear_data.time_period";

// Boolean pref to define the default values for using spellchecker.
const wchar_t kEnableSpellCheck[] = L"browser.enable_spellchecking";

// Boolean pref to define the default values for using auto spell correct.
const wchar_t kEnableAutoSpellCorrect[] = L"browser.enable_autospellcorrect";

// String pref to define the default values for print overlays.
const wchar_t kPrintingPageHeaderLeft[] = L"printing.page.header.left";
const wchar_t kPrintingPageHeaderCenter[] = L"printing.page.header.center";
const wchar_t kPrintingPageHeaderRight[] = L"printing.page.header.right";
const wchar_t kPrintingPageFooterLeft[] = L"printing.page.footer.left";
const wchar_t kPrintingPageFooterCenter[] = L"printing.page.footer.center";
const wchar_t kPrintingPageFooterRight[] = L"printing.page.footer.right";
#if defined(TOOLKIT_USES_GTK)
// GTK specific preference on whether we should match the system GTK theme.
const wchar_t kUsesSystemTheme[] = L"extensions.theme.use_system";
#endif
const wchar_t kCurrentThemePackFilename[] = L"extensions.theme.pack";
const wchar_t kCurrentThemeID[] = L"extensions.theme.id";
const wchar_t kCurrentThemeImages[] = L"extensions.theme.images";
const wchar_t kCurrentThemeColors[] = L"extensions.theme.colors";
const wchar_t kCurrentThemeTints[] = L"extensions.theme.tints";
const wchar_t kCurrentThemeDisplayProperties[] =
    L"extensions.theme.properties";

// Boolean pref which persists whether the extensions_ui is in developer mode
// (showing developer packing tools and extensions details)
const wchar_t kExtensionsUIDeveloperMode[] = L"extensions.ui.developer_mode";

// Integer pref that tracks the number of browser actions visible in the browser
// actions toolbar.
const wchar_t kExtensionToolbarSize[] = L"extensions.toolbarsize";

// Pref containing the directory for internal plugins as written to the plugins
// list (below).
const wchar_t kPluginsLastInternalDirectory[] =
    L"plugins.last_internal_directory";

// List pref containing information (dictionaries) on plugins.
const wchar_t kPluginsPluginsList[] = L"plugins.plugins_list";

// List pref containing names of plugins that are disabled by policy.
const wchar_t kPluginsPluginsBlacklist[] = L"plugins.plugins_blacklist";

// When first shipped, the pdf plugin will be disabled by default.  When we
// enable it by default, we'll want to do so only once.
const wchar_t kPluginsEnabledInternalPDF[] = L"plugins.enabled_internal_pdf";

// Boolean that indicates whether we should check if we are the default browser
// on start-up.
const wchar_t kCheckDefaultBrowser[] = L"browser.check_default_browser";

#if defined(OS_MACOSX)
// Boolean that indicates whether the application should show the info bar
// asking the user to set up automatic updates when Keystone promotion is
// required.
const wchar_t kShowUpdatePromotionInfoBar[] =
    L"browser.show_update_promotion_info_bar";
#endif

// Boolean that is false if we should show window manager decorations.  If
// true, we draw a custom chrome frame (thicker title bar and blue border).
const wchar_t kUseCustomChromeFrame[] = L"browser.custom_chrome_frame";

// Boolean that indicates whether the infobar explaining that search can be
// done directly from the omnibox should be shown.
const wchar_t kShowOmniboxSearchHint[] = L"browser.show_omnibox_search_hint";

// Int which specifies how many times left to show a promotional message on the
// NTP.  This value decrements each time the NTP is shown for the first time
// in a session.
const wchar_t kNTPPromoViewsRemaining[] = L"browser.ntp.promo_remaining";

// The list of origins which are allowed|denied to show desktop notifications.
const wchar_t kDesktopNotificationDefaultContentSetting[] =
    L"profile.notifications_default_content_setting";
const wchar_t kDesktopNotificationAllowedOrigins[] =
    L"profile.notification_allowed_sites";
const wchar_t kDesktopNotificationDeniedOrigins[] =
    L"profile.notification_denied_sites";

// Dictionary of content settings applied to all hosts by default.
const wchar_t kDefaultContentSettings[] = L"profile.default_content_settings";

// OBSOLETE. Dictionary that maps hostnames to content related settings.
// Default settings will be applied to hosts not in this pref.
const wchar_t kPerHostContentSettings[] = L"profile.per_host_content_settings";

// Version of the pattern format used to define content settings.
const wchar_t kContentSettingsVersion[] =
    L"profile.content_settings.pref_version";

// Patterns for mapping hostnames to content related settings. Default settings
// will be applied to hosts that don't match any of the patterns. Replaces
// kPerHostContentSettings. The pattern format used is defined by
// kContentSettingsVersion.
const wchar_t kContentSettingsPatterns[] =
    L"profile.content_settings.patterns";

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const wchar_t kBlockThirdPartyCookies[] = L"profile.block_third_party_cookies";

// Boolean that is true when all locally stored site data (e.g. cookies, local
// storage, etc..) should be deleted on exit.
const wchar_t kClearSiteDataOnExit[] = L"profile.clear_site_data_on_exit";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const wchar_t kPerHostZoomLevels[] = L"profile.per_host_zoom_levels";

// Boolean that is true if AutoFill is enabled and allowed to save profile data.
const wchar_t kAutoFillEnabled[] = L"autofill.enabled";

// Boolean that is true when auxiliary AutoFill profiles are enabled.
// Currently applies to Address Book "me" card on Mac.  False on Win and Linux.
const wchar_t kAutoFillAuxiliaryProfilesEnabled[] =
    L"autofill.auxiliary_profiles_enabled";

// Position and size of the AutoFill dialog.
const wchar_t kAutoFillDialogPlacement[] = L"autofill.dialog_placement";

// Double that indicates positive (for matched forms) upload rate.
const wchar_t kAutoFillPositiveUploadRate[] = L"autofill.positive_upload_rate";

// Double that indicates negative (for not matched forms) upload rate.
const wchar_t kAutoFillNegativeUploadRate[] = L"autofill.negative_upload_rate";

// Boolean that is true when the tabstrip is to be laid out vertically down the
// side of the browser window.
const wchar_t kUseVerticalTabs[] = L"tabs.use_vertical_tabs";

// Boolean that is true when the translate feature is enabled.
const wchar_t kEnableTranslate[] = L"translate.enabled";

const wchar_t kPinnedTabs[] = L"pinned_tabs";

// Integer containing the default Geolocation content setting.
const wchar_t kGeolocationDefaultContentSetting[] =
    L"geolocation.default_content_setting";

// Dictionary that maps [frame, toplevel] to their Geolocation content setting.
const wchar_t kGeolocationContentSettings[] = L"geolocation.content_settings";

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

// The metrics client GUID and session ID.
const wchar_t kMetricsClientID[] = L"user_experience_metrics.client_id";
const wchar_t kMetricsSessionID[] = L"user_experience_metrics.session_id";

// Date/time when the current metrics profile ID was created
// (which hopefully corresponds to first run).
const wchar_t kMetricsClientIDTimestamp[] =
    L"user_experience_metrics.client_id_timestamp";

// Boolean that specifies whether or not crash reporting and metrics reporting
// are sent over the network for analysis.
const wchar_t kMetricsReportingEnabled[] =
    L"user_experience_metrics.reporting_enabled";

// Array of strings that are each UMA logs that were supposed to be sent in the
// first minute of a browser session. These logs include things like crash count
// info, etc.
const wchar_t kMetricsInitialLogs[] =
    L"user_experience_metrics.initial_logs";

// Array of strings that are each UMA logs that were not sent because the
// browser terminated before these accumulated metrics could be sent.  These
// logs typically include histograms and memory reports, as well as ongoing
// user activities.
const wchar_t kMetricsOngoingLogs[] =
    L"user_experience_metrics.ongoing_logs";

// Where profile specific metrics are placed.
const wchar_t kProfileMetrics[] = L"user_experience_metrics.profiles";

// The metrics for a profile are stored as dictionary values under the
// path kProfileMetrics. The individual metrics are placed under the path
// kProfileMetrics.kProfilePrefix<hashed-profile-id>.
const wchar_t kProfilePrefix[] = L"profile-";

// True if the previous run of the program exited cleanly.
const wchar_t kStabilityExitedCleanly[] =
    L"user_experience_metrics.stability.exited_cleanly";

// Version string of previous run, which is used to assure that stability
// metrics reported under current version reflect stability of the same version.
const wchar_t kStabilityStatsVersion[] =
    L"user_experience_metrics.stability.stats_version";

// Build time, in seconds since an epoch, which is used to assure that stability
// metrics reported reflect stability of the same build.
const wchar_t kStabilityStatsBuildTime[] =
    L"user_experience_metrics.stability.stats_buildtime";

// False if we received a session end and either we crashed during processing
// the session end or ran out of time and windows terminated us.
const wchar_t kStabilitySessionEndCompleted[] =
    L"user_experience_metrics.stability.session_end_completed";

// Number of times the application was launched since last report.
const wchar_t kStabilityLaunchCount[] =
    L"user_experience_metrics.stability.launch_count";

// Number of times the application exited uncleanly since the last report.
const wchar_t kStabilityCrashCount[] =
    L"user_experience_metrics.stability.crash_count";

// Number of times the session end did not complete.
const wchar_t kStabilityIncompleteSessionEndCount[] =
    L"user_experience_metrics.stability.incomplete_session_end_count";

// Number of times a page load event occurred since the last report.
const wchar_t kStabilityPageLoadCount[] =
    L"user_experience_metrics.stability.page_load_count";

// Number of times a renderer process crashed since the last report.
const wchar_t kStabilityRendererCrashCount[] =
    L"user_experience_metrics.stability.renderer_crash_count";

// Number of times an extension renderer process crashed since the last report.
const wchar_t kStabilityExtensionRendererCrashCount[] =
    L"user_experience_metrics.stability.extension_renderer_crash_count";

// Time when the app was last launched, in seconds since the epoch.
const wchar_t kStabilityLaunchTimeSec[] =
    L"user_experience_metrics.stability.launch_time_sec";

// Time when the app was last known to be running, in seconds since
// the epoch.
const wchar_t kStabilityLastTimestampSec[] =
    L"user_experience_metrics.stability.last_timestamp_sec";

// This is the location of a list of dictionaries of plugin stability stats.
const wchar_t kStabilityPluginStats[] =
    L"user_experience_metrics.stability.plugin_stats2";

// Number of times the renderer has become non-responsive since the last
// report.
const wchar_t kStabilityRendererHangCount[] =
    L"user_experience_metrics.stability.renderer_hang_count";

// Total number of child process crashes (other than renderer / extension
// renderer ones, and plugin children, which are counted separately) since the
// last report.
const wchar_t kStabilityChildProcessCrashCount[] =
    L"user_experience_metrics.stability.child_process_crash_count";

// Number of times the browser has been able to register crash reporting.
const wchar_t kStabilityBreakpadRegistrationSuccess[] =
    L"user_experience_metrics.stability.breakpad_registration_ok";

// Number of times the browser has failed to register crash reporting.
const wchar_t kStabilityBreakpadRegistrationFail[] =
    L"user_experience_metrics.stability.breakpad_registration_fail";

// Number of times the browser has been run under a debugger.
const wchar_t kStabilityDebuggerPresent[] =
    L"user_experience_metrics.stability.debugger_present";

// Number of times the browser has not been run under a debugger.
const wchar_t kStabilityDebuggerNotPresent[] =
    L"user_experience_metrics.stability.debugger_not_present";

// The keys below are used for the dictionaries in the
// kStabilityPluginStats list.
const wchar_t kStabilityPluginName[] = L"name";
const wchar_t kStabilityPluginLaunches[] = L"launches";
const wchar_t kStabilityPluginInstances[] = L"instances";
const wchar_t kStabilityPluginCrashes[] = L"crashes";

// The keys below are strictly increasing counters over the lifetime of
// a chrome installation. They are (optionally) sent up to the uninstall
// survey in the event of uninstallation.
const wchar_t kUninstallMetricsPageLoadCount[] =
    L"uninstall_metrics.page_load_count";
const wchar_t kUninstallLaunchCount[] = L"uninstall_metrics.launch_count";
const wchar_t kUninstallMetricsInstallDate[] =
    L"uninstall_metrics.installation_date2";
const wchar_t kUninstallMetricsUptimeSec[] = L"uninstall_metrics.uptime_sec";
const wchar_t kUninstallLastLaunchTimeSec[] =
    L"uninstall_metrics.last_launch_time_sec";
const wchar_t kUninstallLastObservedRunTimeSec[] =
    L"uninstall_metrics.last_observed_running_time_sec";

// A collection of position, size, and other data relating to the browser
// window to restore on startup.
const wchar_t kBrowserWindowPlacement[] = L"browser.window_placement";

// A collection of position, size, and other data relating to the task
// manager window to restore on startup.
const wchar_t kTaskManagerWindowPlacement[] = L"task_manager.window_placement";

// A collection of position, size, and other data relating to the page info
// window to restore on startup.
const wchar_t kPageInfoWindowPlacement[] = L"page_info.window_placement";

// A collection of position, size, and other data relating to the keyword
// editor window to restore on startup.
const wchar_t kKeywordEditorWindowPlacement[] =
    L"keyword_editor.window_placement";

// A collection of position, size, and other data relating to the preferences
// window to restore on startup.
const wchar_t kPreferencesWindowPlacement[] = L"preferences.window_placement";

// An integer specifying the total number of bytes to be used by the
// renderer's in-memory cache of objects.
const wchar_t kMemoryCacheSize[] = L"renderer.memory_cache.size";

// String which specifies where to download files to by default.
const wchar_t kDownloadDefaultDirectory[] = L"download.default_directory";

// Boolean that records if the download directory was changed by an
// upgrade a unsafe location to a safe location.
const wchar_t kDownloadDirUpgraded[] = L"download.directory_upgrade";

// String which specifies where to save html files to by default.
const wchar_t kSaveFileDefaultDirectory[] = L"savefile.default_directory";

// String which specifies the last directory that was chosen for uploading
// or opening a file.
extern const wchar_t kSelectFileLastDirectory[] = L"selectfile.last_directory";

// Extensions which should be opened upon completion.
const wchar_t kDownloadExtensionsToOpen[] = L"download.extensions_to_open";

// Integer which specifies the frequency in milliseconds for detecting whether
// plugin windows are hung.
const wchar_t kHungPluginDetectFrequency[] =
    L"browser.hung_plugin_detect_freq";

// Integer which specifies the timeout value to be used for SendMessageTimeout
// to detect a hung plugin window.
const wchar_t kPluginMessageResponseTimeout[] =
    L"browser.plugin_message_response_timeout";

// String which represents the dictionary name for our spell-checker.
const wchar_t kSpellCheckDictionary[] = L"spellcheck.dictionary";

// Dictionary of schemes used by the external protocol handler.
// The value is true if the scheme must be ignored.
const wchar_t kExcludedSchemes[] = L"protocol_handler.excluded_schemes";

// Keys used for MAC handling of SafeBrowsing requests.
const wchar_t kSafeBrowsingClientKey[] = L"safe_browsing.client_key";
const wchar_t kSafeBrowsingWrappedKey[] = L"safe_browsing.wrapped_key";

// Integer that specifies the index of the tab the user was on when they
// last visited the options window.
const wchar_t kOptionsWindowLastTabIndex[] = L"options_window.last_tab_index";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const wchar_t kContentSettingsWindowLastTabIndex[] =
    L"content_settings_window.last_tab_index";

// Integer that specifies the index of the tab the user was on when they
// last visited the Certificate Manager window.
const wchar_t kCertificateManagerWindowLastTabIndex[] =
    L"certificate_manager_window.last_tab_index";

// The mere fact that this pref is registered signals that we should show the
// First Run Search Information bubble when the first browser window appears.
// This preference is only registered by the first-run procedure.
const wchar_t kShouldShowFirstRunBubble[] = L"show-first-run-bubble";

// The mere fact that this pref is registered signals that we should show the
// smaller OEM First Run Search Information bubble when the first
// browser window appears.
// This preference is only registered by the first-run procedure.
const wchar_t kShouldUseOEMFirstRunBubble[] = L"show-OEM-first-run-bubble";

// The mere fact that this pref is registered signals that we should show the
// minimal First Run omnibox information bubble when the first
// browser window appears.
// This preference is only registered by the first-run procedure.
const wchar_t kShouldUseMinimalFirstRunBubble[] =
    L"show-minimal-first-run-bubble";

// Signal that we should show the welcome page when we launch Chrome.
const wchar_t kShouldShowWelcomePage[] = L"show-welcome-page";

// String containing the last known Google URL.  We re-detect this on startup in
// most cases, and use it to send traffic to the correct Google host or with the
// correct Google domain/country code for whatever location the user is in.
const wchar_t kLastKnownGoogleURL[] = L"browser.last_known_google_url";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
const wchar_t kLastKnownIntranetRedirectOrigin[] = L"";

// Integer containing the system Country ID the first time we checked the
// template URL prepopulate data.  This is used to avoid adding a whole bunch of
// new search engine choices if prepopulation runs when the user's Country ID
// differs from their previous Country ID.  This pref does not exist until
// prepopulation has been run at least once.
const wchar_t kCountryIDAtInstall[] = L"countryid_at_install";
// OBSOLETE. Same as above, but uses the Windows-specific GeoID value instead.
// Updated if found to the above key.
const wchar_t kGeoIDAtInstall[] = L"geoid_at_install";

// An enum value of how the browser was shut down (see browser_shutdown.h).
const wchar_t kShutdownType[] = L"shutdown.type";
// Number of processes that were open when the user shut down.
const wchar_t kShutdownNumProcesses[] = L"shutdown.num_processes";
// Number of processes that were shut down using the slow path.
const wchar_t kShutdownNumProcessesSlow[] = L"shutdown.num_processes_slow";

// Whether to restart the current Chrome session automatically as the last thing
// before shutting everything down.
const wchar_t kRestartLastSessionOnShutdown[] =
    L"restart.last.session.on.shutdown";

// Number of bookmarks/folders on the bookmark bar/other bookmark folder.
const wchar_t kNumBookmarksOnBookmarkBar[] =
    L"user_experience_metrics.num_bookmarks_on_bookmark_bar";
const wchar_t kNumFoldersOnBookmarkBar[] =
    L"user_experience_metrics.num_folders_on_bookmark_bar";
const wchar_t kNumBookmarksInOtherBookmarkFolder[] =
    L"user_experience_metrics.num_bookmarks_in_other_bookmark_folder";
const wchar_t kNumFoldersInOtherBookmarkFolder[] =
    L"user_experience_metrics.num_folders_in_other_bookmark_folder";

// Number of keywords.
const wchar_t kNumKeywords[] = L"user_experience_metrics.num_keywords";

// Placeholder preference for disabling voice / video chat if it is ever added.
// Currently, this does not change any behavior.
const wchar_t kDisableVideoAndChat[] = L"disable_video_chat";

// Whether Extensions are enabled.
const wchar_t kDisableExtensions[] = L"extensions.disabled";

// Boolean which specifies whether the Extension Shelf is visible on all tabs.
const wchar_t kShowExtensionShelf[] = L"extensions.shelf.show_on_all_tabs";

// Integer boolean representing the width (in pixels) of the container for
// browser actions.
const wchar_t kBrowserActionContainerWidth[] =
    L"extensions.browseractions.container.width";

// Time of the last, and next scheduled, extensions auto-update checks.
const wchar_t kLastExtensionsUpdateCheck[] =
    L"extensions.autoupdate.last_check";
const wchar_t kNextExtensionsUpdateCheck[] =
    L"extensions.autoupdate.next_check";
// Version number of last blacklist check
const wchar_t kExtensionBlacklistUpdateVersion[] =
    L"extensions.blacklistupdate.version";

// New Tab Page URLs that should not be shown as most visited thumbnails.
const wchar_t kNTPMostVisitedURLsBlacklist[] = L"ntp.most_visited_blacklist";

// The URLs that have been pinned to the Most Visited section of the New Tab
// Page.
const wchar_t kNTPMostVisitedPinnedURLs[] = L"ntp.pinned_urls";

// Data downloaded from resource pages (JSON, RSS) to be displayed in the
// recommendations portion of the NTP.
const wchar_t kNTPTipsCache[] = L"ntp.tips_cache";

// Last time of update of tips_cache.
const wchar_t kNTPTipsCacheUpdate[] = L"ntp.tips_cache_update";

// Last server used to fill tips_cache.
const wchar_t kNTPTipsServer[] = L"ntp.tips_server";

// Which sections should be visible on the new tab page
// 1 - Show the most visited sites in a grid
// 2 - Show the most visited sites as a list
// 4 - Show the recent section
// 8 - Show tips
// 16 - show sync status
const wchar_t kNTPShownSections[] = L"ntp.shown_sections";

// This pref is used for migrating the prefs for the NTP
const wchar_t kNTPPrefVersion[] = L"ntp.pref_version";

// A boolean specifying whether dev tools window should be opened docked.
const wchar_t kDevToolsOpenDocked[] = L"devtools.open_docked";

// Integer location of the split bar in the browser view.
const wchar_t kDevToolsSplitLocation[] = L"devtools.split_location";

// 64-bit integer serialization of the base::Time when the last sync occurred.
const wchar_t kSyncLastSyncedTime[] = L"sync.last_synced_time";

// Boolean specifying whether the user finished setting up sync.
const wchar_t kSyncHasSetupCompleted[] = L"sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const wchar_t kKeepEverythingSynced[] = L"sync.keep_everything_synced";

// Booleans specifying whether the user has selected to sync the following
// datatypes.
const wchar_t kSyncBookmarks[] = L"sync.bookmarks";
const wchar_t kSyncPasswords[] = L"sync.passwords";
const wchar_t kSyncPreferences[] = L"sync.preferences";
const wchar_t kSyncAutofill[] = L"sync.autofill";
const wchar_t kSyncThemes[] = L"sync.themes";
const wchar_t kSyncTypedUrls[] = L"sync.typed_urls";
const wchar_t kSyncExtensions[] = L"sync.extensions";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const wchar_t kSyncManaged[] = L"sync.managed";

// Create web application shortcut dialog preferences.
const wchar_t kWebAppCreateOnDesktop[] = L"browser.web_app.create_on_desktop";
const wchar_t kWebAppCreateInAppsMenu[] =
    L"browser.web_app.create_in_apps_menu";
const wchar_t kWebAppCreateInQuickLaunchBar[] =
    L"browser.web_app.create_in_quick_launch_bar";

// Dictionary that maps Geolocation network provider server URLs to
// corresponding access token.
const wchar_t kGeolocationAccessToken[] = L"geolocation.access_token";

// Whether PasswordForms have been migrated from the WedDataService to the
// LoginDatabase.
const wchar_t kLoginDatabaseMigrated[] = L"login_database.migrated";

// The root URL of the cloud print service.
const wchar_t kCloudPrintServiceURL[] = L"cloud_print.service_url";

// The list of BackgroundContents that should be loaded when the browser
// launches.
const wchar_t kRegisteredBackgroundContents[] =
    L"background_contents.registered";

// *************** SERVICE PREFS ***************
// These are attached to the service process.

// The unique id for this instance of the cloud print proxy.
const wchar_t kCloudPrintProxyId[] = L"cloud_print.proxy_id";
// The GAIA auth token for Cloud Print
const wchar_t kCloudPrintAuthToken[] = L"cloud_print.auth_token";
// The GAIA auth token used by Cloud Print to authenticate with the XMPP server
// This should eventually go away because the above token should work for both.
const wchar_t kCloudPrintXMPPAuthToken[] = L"cloud_print.xmpp_auth_token";
// The email address of the account used to authenticate with the Cloud Print
// server.
extern const wchar_t kCloudPrintEmail[] = L"cloud_print.email";
// Settings specific to underlying print system.
extern const wchar_t kCloudPrintPrintSystemSettings[] =
    L"cloud_print.print_system_settings";

// Boolean to disable proxy altogether. If true, other proxy
// preferences are ignored.
const wchar_t kNoProxyServer[] = L"proxy.disabled";
// Boolean specifying if proxy should be auto-detected.
const wchar_t kProxyAutoDetect[] = L"proxy.auto_detect";
// String specifying the proxy server. For a specification of the expected
// syntax see net::ProxyConfig::ProxyRules::ParseFromString().
const wchar_t kProxyServer[] = L"proxy.server";
// URL to the proxy .pac file.
const wchar_t kProxyPacUrl[] = L"proxy.pac_url";
// String containing proxy bypass rules. For a specification of the
// expected syntax see net::ProxyBypassRules::ParseFromString().
const wchar_t kProxyBypassList[] = L"proxy.bypass_list";

}  // namespace prefs
