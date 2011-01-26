// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// A counter that controls whether the apps promo is shown in the app launcher
// or not.
const char kAppsPromoCounter[] = "apps_promo_counter";

// Whether we have installed default apps yet in this profile.
const char kDefaultAppsInstalled[] = "default_apps_installed";

// A boolean specifying whether the New Tab page is the home page or not.
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const char kHomePage[] = "homepage";

// Used to determine if the last session exited cleanly. Set to false when
// first opened, and to true when closing. On startup if the value is false,
// it means the profile didn't exit cleanly.
const char kSessionExitedCleanly[] = "profile.exited_cleanly";

// An integer pref. Holds one of several values:
// 0: (or empty) don't do anything special on startup.
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
const char kRestoreOnStartup[] = "session.restore_on_startup";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
const char kURLsToRestoreOnStartup[] = "session.urls_to_restore_on_startup";

// The application locale.
// For OS_CHROMEOS we maintain kApplicationLocale property in both local state
// and user's profile.  Global property determines locale of login screen,
// while user's profile determines his personal locale preference.
const char kApplicationLocale[] = "intl.app_locale";
#if defined(OS_CHROMEOS)
// Non-syncable item.  Used to detect locale change.
// Used for two-step initialization of locale in ChromeOS
// because synchronization of kApplicationLocale is not instant.
const char kApplicationLocaleBackup[] = "intl.app_locale_backup";
// Non-syncable item.
// Used to locally override synchronized kApplicationLocale preference.
const char kApplicationLocaleOverride[] = "intl.app_locale_override";
// Locale accepted by user.  Non-syncable.
// Used to determine whether we need to show Locale Change notification.
const char kApplicationLocaleAccepted[] = "intl.app_locale_accepted";
#endif

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
const char kDefaultCharset[] = "intl.charset_default";

// The value to use for Accept-Languages HTTP header when making an HTTP
// request.
const char kAcceptLanguages[] = "intl.accept_languages";

// The value to use for showing locale-dependent encoding list for different
// locale, it's initialized from the corresponding string resource that is
// stored in non-translatable part of the resource bundle.
const char kStaticEncodings[] = "intl.static_encodings";

// OBSOLETE.  The list of hostnames for which we whitelist popups (rather than
// blocking).
const char kPopupWhitelistedHosts[] = "profile.popup_whitelisted_sites";

// WebKit preferences.
// A boolean flag to indicate whether WebKit standard font family is
// serif or sans-serif. We don't have a UI for setting standard family.
// Instead, we use this pref to map either serif or sans_serif to WebKit
// standard font family. At the moment, we don't have a UI for this
// flag, either.
const char kWebKitStandardFontIsSerif[] =
    "webkit.webprefs.standard_font_is_serif";
const char kWebKitFixedFontFamily[] = "webkit.webprefs.fixed_font_family";
const char kWebKitSerifFontFamily[] = "webkit.webprefs.serif_font_family";
const char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.sansserif_font_family";
const char kWebKitCursiveFontFamily[] = "webkit.webprefs.cursive_font_family";
const char kWebKitFantasyFontFamily[] = "webkit.webprefs.fantasy_font_family";
const char kWebKitDefaultFontSize[] = "webkit.webprefs.default_font_size";
const char kWebKitDefaultFixedFontSize[] =
    "webkit.webprefs.default_fixed_font_size";
const char kWebKitMinimumFontSize[] = "webkit.webprefs.minimum_font_size";
const char kWebKitMinimumLogicalFontSize[] =
    "webkit.webprefs.minimum_logical_font_size";
const char kWebKitJavascriptEnabled[] = "webkit.webprefs.javascript_enabled";
const char kWebKitWebSecurityEnabled[] = "webkit.webprefs.web_security_enabled";
const char kWebKitJavascriptCanOpenWindowsAutomatically[] =
    "webkit.webprefs.javascript_can_open_windows_automatically";
const char kWebKitLoadsImagesAutomatically[] =
    "webkit.webprefs.loads_images_automatically";
const char kWebKitPluginsEnabled[] = "webkit.webprefs.plugins_enabled";
const char kWebKitDomPasteEnabled[] = "webkit.webprefs.dom_paste_enabled";
const char kWebKitShrinksStandaloneImagesToFit[] =
    "webkit.webprefs.shrinks_standalone_images_to_fit";
const char kWebKitInspectorSettings[] = "webkit.webprefs.inspector_settings";
const char kWebKitUsesUniversalDetector[] =
    "webkit.webprefs.uses_universal_detector";
const char kWebKitTextAreasAreResizable[] =
    "webkit.webprefs.text_areas_are_resizable";
const char kWebKitJavaEnabled[] = "webkit.webprefs.java_enabled";
const char kWebkitTabsToLinks[] = "webkit.webprefs.tabs_to_links";

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";

// Boolean that is true if the password manager is on (will record new
// passwords and fill in known passwords).
const char kPasswordManagerEnabled[] = "profile.password_manager_enabled";

// Boolean controlling whether the password manager allows to retrieve passwords
// in clear text.
const char kPasswordManagerAllowShowPasswords[] =
    "profile.password_manager_allow_show_passwords";

// OBSOLETE.  Boolean that is true if the form AutoFill is on (will record
// values entered in text inputs in forms and shows them in a popup when user
// type in a text input with the same name later on).  This has been superseded
// by kAutoFillEnabled.
const char kFormAutofillEnabled[] = "profile.form_autofill_enabled";

// Boolean that is true when SafeBrowsing is enabled.
const char kSafeBrowsingEnabled[] = "safebrowsing.enabled";

// Boolean that is true when SafeBrowsing Malware Report is enabled.
const char kSafeBrowsingReportingEnabled[] =
    "safebrowsing.reporting_enabled";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// OBSOLETE.  Enum that specifies whether to enforce a third-party cookie
// blocking policy.  This has been superseded by kDefaultContentSettings +
// kBlockThirdPartyCookies.
// 0 - allow all cookies.
// 1 - block third-party cookies
// 2 - block all cookies
const char kCookieBehavior[] = "security.cookie_behavior";

// Whether having a default search provider is enabled.
const char kDefaultSearchProviderEnabled[] =
    "default_search_provider.enabled";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for searches.
const char kDefaultSearchProviderSearchURL[] =
    "default_search_provider.search_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for suggestions.
const char kDefaultSearchProviderSuggestURL[] =
    "default_search_provider.suggest_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for instant results.
const char kDefaultSearchProviderInstantURL[] =
    "default_search_provider.instant_url";

// The Fav Icon URL (as understood by TemplateURLRef) of the default search
// provider.
const char kDefaultSearchProviderIconURL[] =
    "default_search_provider.icon_url";

// The input encoding (as understood by TemplateURLRef) supported by the default
// search provider.  The various encodings are separated by ';'
const char kDefaultSearchProviderEncodings[] =
    "default_search_provider.encodings";

// The name of the default search provider.
const char kDefaultSearchProviderName[] = "default_search_provider.name";

// The keyword of the default search provider.
const char kDefaultSearchProviderKeyword[] = "default_search_provider.keyword";

// The id of the default search provider.
const char kDefaultSearchProviderID[] = "default_search_provider.id";

// The prepopulate id of the default search provider.
const char kDefaultSearchProviderPrepopulateID[] =
    "default_search_provider.prepopulate_id";

// The dictionary key used when the default search providers are given
// in the preferences file. Normally they are copied from the master
// preferences file.
const char kSearchProviderOverrides[] = "search_provider_overrides";
// The format version for the dictionary above.
const char kSearchProviderOverridesVersion[] =
    "search_provider_overrides_version";

// Boolean which specifies whether we should ask the user if we should download
// a file (true) or just download it automatically.
const char kPromptForDownload[] = "download.prompt_for_download";

// A boolean pref set to true if we're using Link Doctor error pages.
const char kAlternateErrorPagesEnabled[] = "alternate_error_pages.enabled";

// A boolean pref set to true if DNS pre-fetching is being done in browser.
const char kDnsPrefetchingEnabled[] = "dns_prefetching.enabled";

// OBSOLETE: new pref now stored with user prefs instead of profile, as
// kDnsPrefetchingStartupList.
const char kDnsStartupPrefetchList[] = "StartupDNSPrefetchList";

// An adaptively identified list of domain names to be pre-fetched during the
// next startup, based on what was actually needed during this startup.
const char kDnsPrefetchingStartupList[] = "dns_prefetching.startup_list";

// OBSOLETE: new pref now stored with user prefs instead of profile, as
// kDnsPrefetchingHostReferralList.
const char kDnsHostReferralList[] = "HostReferralList";

// A list of host names used to fetch web pages, and their commonly used
// sub-resource hostnames (and expected latency benefits from pre-resolving, or
// preconnecting to, such sub-resource hostnames).
// This list is adaptively grown and pruned.
const char kDnsPrefetchingHostReferralList[] =
    "dns_prefetching.host_referral_list";

// Disables the SPDY protocol.
const char kDisableSpdy[] = "spdy.disabled";

// Is the cookie prompt expanded?
const char kCookiePromptExpanded[] = "cookieprompt.expanded";

// Boolean pref indicating whether the instant confirm dialog has been shown.
const char kInstantConfirmDialogShown[] = "instant.confirm_dialog_shown";

// Boolean pref indicating if instant is enabled.
const char kInstantEnabled[] = "instant.enabled";

// Boolean pref indicating if instant was ever enabled.
const char kInstantEnabledOnce[] = "instant.enabled_once";

// Time when instant was last enabled.
const char kInstantEnabledTime[] = "instant.enabled_time";

// Used to maintain instant promo keys. See PromoCounter for details of subkeys
// that are used.
const char kInstantPromo[] = "instant.promo";

// Used to migrate preferences from local state to user preferences to
// enable multiple profiles.
// Holds possible values:
// 0: no preferences migrated yet.
// 1: dns prefetching preferences stored in user preferences.
const char kMultipleProfilePrefMigration[] =
    "profile.multiple_profile_prefs_version";

#if defined(USE_NSS) || defined(USE_OPENSSL)
// Prefs for SSLConfigServicePref.  Currently, these are only present on
// and used by NSS/OpenSSL using OSes.
const char kCertRevocationCheckingEnabled[] = "ssl.rev_checking.enabled";
const char kSSL3Enabled[] = "ssl.ssl3.enabled";
const char kTLS1Enabled[] = "ssl.tls1.enabled";
#endif

#if defined(OS_CHROMEOS)
// An integer pref to initially mute volume if 1.
const char kAudioMute[] = "settings.audio.mute";

// A double pref to set initial volume.
const char kAudioVolume[] = "settings.audio.volume";

// A boolean pref set to true if TapToClick is being done in browser.
const char kTapToClickEnabled[] = "settings.touchpad.enable_tap_to_click";

// A integer pref for the touchpad sensitivity.
const char kTouchpadSensitivity[] = "settings.touchpad.sensitivity2";

// A string pref set to the current input method.
const char kLanguageCurrentInputMethod[] =
    "settings.language.current_input_method";

// A string pref set to the previous input method.
const char kLanguagePreviousInputMethod[] =
    "settings.language.previous_input_method";

// A string pref (comma-separated list) set to the "next engine in menu"
// hot-key lists.
const char kLanguageHotkeyNextEngineInMenu[] =
    "settings.language.hotkey_next_engine_in_menu";

// A string pref (comma-separated list) set to the "previous engine"
// hot-key lists.
const char kLanguageHotkeyPreviousEngine[] =
    "settings.language.hotkey_previous_engine";

// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const char kLanguagePreferredLanguages[] =
    "settings.language.preferred_languages";

// A string pref (comma-separated list) set to the preloaded (active) input
// method IDs (ex. "pinyin,mozc").
const char kLanguagePreloadEngines[] = "settings.language.preload_engines";

// Boolean prefs for ibus-chewing Chinese input method.
const char kLanguageChewingAutoShiftCur[] =
    "settings.language.chewing_auto_shift_cur";
const char kLanguageChewingAddPhraseDirection[] =
    "settings.language.chewing_add_phrase_direction";
const char kLanguageChewingEasySymbolInput[] =
    "settings.language.chewing_easy_symbol_input";
const char kLanguageChewingEscCleanAllBuf[] =
    "settings.language.chewing_esc_clean_all_buf";
const char kLanguageChewingForceLowercaseEnglish[] =
    "settings.language.chewing_force_lowercase_english";
const char kLanguageChewingPlainZhuyin[] =
    "settings.language.chewing_plain_zhuyin";
const char kLanguageChewingPhraseChoiceRearward[] =
    "settings.language.chewing_phrase_choice_rearward";
const char kLanguageChewingSpaceAsSelection[] =
    "settings.language.chewing_space_as_selection";

// Integer prefs for ibus-chewing Chinese input method.
const char kLanguageChewingMaxChiSymbolLen[] =
    "settings.language.chewing_max_chi_symbol_len";
const char kLanguageChewingCandPerPage[] =
    "settings.language.chewing_cand_per_page";

// String prefs for ibus-chewing Chinese input method.
const char kLanguageChewingKeyboardType[] =
    "settings.language.chewing_keyboard_type";
const char kLanguageChewingSelKeys[] =
    "settings.language.chewing_sel_keys";

const char kLanguageChewingHsuSelKeyType[] =
    "settings.language.chewing_hsu_sel_key_type";

// A string pref which determines the keyboard layout for Hangul input method.
const char kLanguageHangulKeyboard[] = "settings.language.hangul_keyboard";
const char kLanguageHangulHanjaKeys[] = "settings.language.hangul_hanja_keys";

// A boolean prefs for ibus-pinyin Chinese input method.
const char kLanguagePinyinCorrectPinyin[] =
    "settings.language.pinyin_correct_pinyin";
const char kLanguagePinyinFuzzyPinyin[] =
    "settings.language.pinyin_fuzzy_pinyin";
const char kLanguagePinyinShiftSelectCandidate[] =
    "settings.language.pinyin_shift_select_candidate";
const char kLanguagePinyinMinusEqualPage[] =
    "settings.language.pinyin_minus_equal_page";
const char kLanguagePinyinCommaPeriodPage[] =
    "settings.language.pinyin_comma_period_page";
const char kLanguagePinyinAutoCommit[] =
    "settings.language.pinyin_auto_commit";
const char kLanguagePinyinDoublePinyin[] =
    "settings.language.pinyin_double_pinyin";
const char kLanguagePinyinInitChinese[] =
    "settings.language.pinyin_init_chinese";
const char kLanguagePinyinInitFull[] =
    "settings.language.pinyin_init_full";
const char kLanguagePinyinInitFullPunct[] =
    "settings.language.pinyin_init_full_punct";
const char kLanguagePinyinInitSimplifiedChinese[] =
    "settings.language.pinyin_init_simplified_chinese";
const char kLanguagePinyinTradCandidate[] =
    "settings.language.pinyin_trad_candidate";

// A integer prefs for ibus-pinyin Chinese input method.
const char kLanguagePinyinDoublePinyinSchema[] =
    "settings.language.pinyin_double_pinyin_schema";
const char kLanguagePinyinLookupTablePageSize[] =
    "settings.language.pinyin_lookup_table_page_size";

// A string prefs for ibus-mozc Japanese input method.
// ibus-mozc converts the string values to protobuf enum values defined in
// third_party/ibus-mozc/files/src/session/config.proto.
const char kLanguageMozcPreeditMethod[] =
    "settings.language.mozc_preedit_method";
const char kLanguageMozcSessionKeymap[] =
    "settings.language.mozc_session_keymap";
const char kLanguageMozcPunctuationMethod[] =
    "settings.language.mozc_punctuation_method";
const char kLanguageMozcSymbolMethod[] =
    "settings.language.mozc_symbol_method";
const char kLanguageMozcSpaceCharacterForm[] =
    "settings.language.mozc_space_character_form";
const char kLanguageMozcHistoryLearningLevel[] =
    "settings.language.mozc_history_learning_level";
const char kLanguageMozcSelectionShortcut[] =
    "settings.language.mozc_selection_shortcut";
const char kLanguageMozcShiftKeyModeSwitch[] =
    "settings.language.mozc_shift_key_mode_switch";
const char kLanguageMozcNumpadCharacterForm[] =
    "settings.language.mozc_numpad_character_form";
const char kLanguageMozcIncognitoMode[] =
    "settings.language.mozc_incognito_mode";
const char kLanguageMozcUseAutoImeTurnOff[] =
    "settings.language.mozc_use_auto_ime_turn_off";
const char kLanguageMozcUseDateConversion[] =
    "settings.language.mozc_use_date_conversion";
const char kLanguageMozcUseSingleKanjiConversion[] =
    "settings.language.mozc_use_single_kanji_conversion";
const char kLanguageMozcUseSymbolConversion[] =
    "settings.language.mozc_use_symbol_conversion";
const char kLanguageMozcUseNumberConversion[] =
    "settings.language.mozc_use_number_conversion";
const char kLanguageMozcUseHistorySuggest[] =
    "settings.language.mozc_use_history_suggest";
const char kLanguageMozcUseDictionarySuggest[] =
    "settings.language.mozc_use_dictionary_suggest";
const char kLanguageMozcSuggestionsSize[] =
    "settings.language.mozc_suggestions_size";

// A integer prefs which determine how we remap modifier keys (e.g. swap Alt-L
// and Control-L.) Possible values for these prefs are 0-4. See ModifierKey enum
// in src/third_party/cros/chrome_keyboard.h for details.
const char kLanguageXkbRemapSearchKeyTo[] =
    "settings.language.xkb_remap_search_key_to";
const char kLanguageXkbRemapControlKeyTo[] =
    "settings.language.xkb_remap_control_key_to";
const char kLanguageXkbRemapAltKeyTo[] =
    "settings.language.xkb_remap_alt_key_to";

// A boolean pref which determines whether key repeat is enabled.
const char kLanguageXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";
// A integer pref which determines key repeat delay (in ms).
const char kLanguageXkbAutoRepeatDelay[] =
    "settings.language.xkb_auto_repeat_delay_r2";
// A integer pref which determines key repeat interval (in ms).
const char kLanguageXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes are added to the three prefs above when we change the
// preferences not user-configurable, not to sync them with cloud.

// A boolean pref which determines whether accessibility is enabled.
const char kAccessibilityEnabled[] = "settings.accessibility";

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
const char kLabsAdvancedFilesystemEnabled[] =
    "settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
const char kLabsMediaplayerEnabled[] = "settings.labs.mediaplayer";

// A boolean pref that turns on screen locker.
const char kEnableScreenLock[] = "settings.enable_screen_lock";

// A boolean pref of whether to show mobile plan notifications.
const char kShowPlanNotifications[] =
    "settings.internet.mobile.show_plan_notifications";

#endif  // defined(OS_CHROMEOS)

// The disabled messages in IPC logging.
const char kIpcDisabledMessages[] = "ipc_log_disabled_messages";

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
const char kShowHomeButton[] = "browser.show_home_button";

// A boolean pref set to true if the Page and Options menu buttons should be
// visible on the toolbar. This is only used for Mac where the default is to
// have these menu in the main menubar, not as buttons on the toolbar.
const char kShowPageOptionsButtons[] = "browser.show_page_options_buttons";

// A string value which saves short list of recently user selected encodings
// separated with comma punctuation mark.
const char kRecentlySelectedEncoding[] = "profile.recently_selected_encodings";

// Clear Browsing Data dialog preferences.
const char kDeleteBrowsingHistory[] = "browser.clear_data.browsing_history";
const char kDeleteDownloadHistory[] = "browser.clear_data.download_history";
const char kDeleteCache[] = "browser.clear_data.cache";
const char kDeleteCookies[] = "browser.clear_data.cookies";
const char kDeletePasswords[] = "browser.clear_data.passwords";
const char kDeleteFormData[] = "browser.clear_data.form_data";
const char kDeleteTimePeriod[] = "browser.clear_data.time_period";

// Whether there is a Flash version installed that supports clearing LSO data.
const char kClearPluginLSODataEnabled[] = "browser.clear_lso_data_enabled";

// Boolean pref to define the default values for using spellchecker.
const char kEnableSpellCheck[] = "browser.enable_spellchecking";

// List of names of the enabled labs experiments (see chrome/browser/labs.cc).
const char kEnabledLabsExperiments[] = "browser.enabled_labs_experiments";

// Boolean pref to define the default values for using auto spell correct.
const char kEnableAutoSpellCorrect[] = "browser.enable_autospellcorrect";

// Boolean controlling whether history saving is disabled.
const char kSavingBrowserHistoryDisabled[] = "history.saving_disabled";

// Boolean controlling whether printing is enabled.
const char kPrintingEnabled[] = "printing.enabled";

// String pref to define the default values for print overlays.
const char kPrintingPageHeaderLeft[] = "printing.page.header.left";
const char kPrintingPageHeaderCenter[] = "printing.page.header.center";
const char kPrintingPageHeaderRight[] = "printing.page.header.right";
const char kPrintingPageFooterLeft[] = "printing.page.footer.left";
const char kPrintingPageFooterCenter[] = "printing.page.footer.center";
const char kPrintingPageFooterRight[] = "printing.page.footer.right";
#if defined(TOOLKIT_USES_GTK)
// GTK specific preference on whether we should match the system GTK theme.
const char kUsesSystemTheme[] = "extensions.theme.use_system";
#endif
const char kCurrentThemePackFilename[] = "extensions.theme.pack";
const char kCurrentThemeID[] = "extensions.theme.id";
const char kCurrentThemeImages[] = "extensions.theme.images";
const char kCurrentThemeColors[] = "extensions.theme.colors";
const char kCurrentThemeTints[] = "extensions.theme.tints";
const char kCurrentThemeDisplayProperties[] = "extensions.theme.properties";

// Boolean pref which persists whether the extensions_ui is in developer mode
// (showing developer packing tools and extensions details)
const char kExtensionsUIDeveloperMode[] = "extensions.ui.developer_mode";

// Integer pref that tracks the number of browser actions visible in the browser
// actions toolbar.
const char kExtensionToolbarSize[] = "extensions.toolbarsize";

// Pref containing the directory for internal plugins as written to the plugins
// list (below).
const char kPluginsLastInternalDirectory[] = "plugins.last_internal_directory";

// List pref containing information (dictionaries) on plugins.
const char kPluginsPluginsList[] = "plugins.plugins_list";

// List pref containing names of plugins that are disabled by policy.
const char kPluginsPluginsBlacklist[] = "plugins.plugins_blacklist";

// When first shipped, the pdf plugin will be disabled by default.  When we
// enable it by default, we'll want to do so only once.
const char kPluginsEnabledInternalPDF[] = "plugins.enabled_internal_pdf3";

// Boolean that indicates whether we should check if we are the default browser
// on start-up.
const char kCheckDefaultBrowser[] = "browser.check_default_browser";

#if defined(OS_MACOSX)
// Boolean that indicates whether the application should show the info bar
// asking the user to set up automatic updates when Keystone promotion is
// required.
const char kShowUpdatePromotionInfoBar[] =
    "browser.show_update_promotion_info_bar";
#endif

// Boolean that is false if we should show window manager decorations.  If
// true, we draw a custom chrome frame (thicker title bar and blue border).
const char kUseCustomChromeFrame[] = "browser.custom_chrome_frame";

// Boolean that indicates whether the infobar explaining that search can be
// done directly from the omnibox should be shown.
const char kShowOmniboxSearchHint[] = "browser.show_omnibox_search_hint";

// The list of origins which are allowed|denied to show desktop notifications.
const char kDesktopNotificationDefaultContentSetting[] =
    "profile.notifications_default_content_setting";
const char kDesktopNotificationAllowedOrigins[] =
    "profile.notification_allowed_sites";
const char kDesktopNotificationDeniedOrigins[] =
    "profile.notification_denied_sites";

// The preferred position (which corner of screen) for desktop notifications.
const char kDesktopNotificationPosition[] =
    "browser.desktop_notification_position";

// Dictionary of content settings applied to all hosts by default.
const char kDefaultContentSettings[] = "profile.default_content_settings";

// OBSOLETE. Dictionary that maps hostnames to content related settings.
// Default settings will be applied to hosts not in this pref.
const char kPerHostContentSettings[] = "profile.per_host_content_settings";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Patterns for mapping hostnames to content related settings. Default settings
// will be applied to hosts that don't match any of the patterns. Replaces
// kPerHostContentSettings. The pattern format used is defined by
// kContentSettingsVersion.
const char kContentSettingsPatterns[] = "profile.content_settings.patterns";

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const char kBlockThirdPartyCookies[] = "profile.block_third_party_cookies";

// Boolean that is true if non-sandboxed plug-ins should be blocked.
const char kBlockNonsandboxedPlugins[] = "profile.block_nonsandboxed_plugins";

// Boolean that is true when all locally stored site data (e.g. cookies, local
// storage, etc..) should be deleted on exit.
const char kClearSiteDataOnExit[] = "profile.clear_site_data_on_exit";

// Double that indicates the default zoom level.
const char kDefaultZoomLevel[] = "profile.default_zoom_level";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const char kPerHostZoomLevels[] = "profile.per_host_zoom_levels";

// Boolean that is true if AutoFill is enabled and allowed to save profile data.
const char kAutoFillEnabled[] = "autofill.enabled";

// Boolean that is true when auxiliary AutoFill profiles are enabled.
// Currently applies to Address Book "me" card on Mac.  False on Win and Linux.
const char kAutoFillAuxiliaryProfilesEnabled[] =
    "autofill.auxiliary_profiles_enabled";

// Position and size of the AutoFill dialog.
const char kAutoFillDialogPlacement[] = "autofill.dialog_placement";

// Double that indicates positive (for matched forms) upload rate.
const char kAutoFillPositiveUploadRate[] = "autofill.positive_upload_rate";

// Double that indicates negative (for not matched forms) upload rate.
const char kAutoFillNegativeUploadRate[] = "autofill.negative_upload_rate";

// Boolean option set to true on the first run. Non-persistent.
const char kAutoFillPersonalDataManagerFirstRun[] = "autofill.pdm.first_run";

// Boolean that is true when the tabstrip is to be laid out vertically down the
// side of the browser window.
const char kUseVerticalTabs[] = "tabs.use_vertical_tabs";

// Boolean that is true when the translate feature is enabled.
const char kEnableTranslate[] = "translate.enabled";

const char kPinnedTabs[] = "pinned_tabs";

// Integer that specifies the policy refresh rate in milliseconds. Not all
// values are meaningful, so it is clamped to a sane range by the policy
// provider.
const char kPolicyRefreshRate[] = "policy.refresh_rate";

// Integer containing the default Geolocation content setting.
const char kGeolocationDefaultContentSetting[] =
    "geolocation.default_content_setting";

// Dictionary that maps [frame, toplevel] to their Geolocation content setting.
const char kGeolocationContentSettings[] = "geolocation.content_settings";

// Preference to disable 3D APIs (WebGL, Pepper 3D).
const char kDisable3DAPIs[] = "disable_3d_apis";

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

// The metrics client GUID and session ID.
const char kMetricsClientID[] = "user_experience_metrics.client_id";
const char kMetricsSessionID[] = "user_experience_metrics.session_id";

// Date/time when the current metrics profile ID was created
// (which hopefully corresponds to first run).
const char kMetricsClientIDTimestamp[] =
    "user_experience_metrics.client_id_timestamp";

// Boolean that specifies whether or not crash reporting and metrics reporting
// are sent over the network for analysis.
const char kMetricsReportingEnabled[] =
    "user_experience_metrics.reporting_enabled";

// Array of strings that are each UMA logs that were supposed to be sent in the
// first minute of a browser session. These logs include things like crash count
// info, etc.
const char kMetricsInitialLogs[] =
    "user_experience_metrics.initial_logs";

// Array of strings that are each UMA logs that were not sent because the
// browser terminated before these accumulated metrics could be sent.  These
// logs typically include histograms and memory reports, as well as ongoing
// user activities.
const char kMetricsOngoingLogs[] =
    "user_experience_metrics.ongoing_logs";

// Where profile specific metrics are placed.
const char kProfileMetrics[] = "user_experience_metrics.profiles";

// The metrics for a profile are stored as dictionary values under the
// path kProfileMetrics. The individual metrics are placed under the path
// kProfileMetrics.kProfilePrefix<hashed-profile-id>.
const char kProfilePrefix[] = "profile-";

// True if the previous run of the program exited cleanly.
const char kStabilityExitedCleanly[] =
    "user_experience_metrics.stability.exited_cleanly";

// Version string of previous run, which is used to assure that stability
// metrics reported under current version reflect stability of the same version.
const char kStabilityStatsVersion[] =
    "user_experience_metrics.stability.stats_version";

// Build time, in seconds since an epoch, which is used to assure that stability
// metrics reported reflect stability of the same build.
const char kStabilityStatsBuildTime[] =
    "user_experience_metrics.stability.stats_buildtime";

// False if we received a session end and either we crashed during processing
// the session end or ran out of time and windows terminated us.
const char kStabilitySessionEndCompleted[] =
    "user_experience_metrics.stability.session_end_completed";

// Number of times the application was launched since last report.
const char kStabilityLaunchCount[] =
    "user_experience_metrics.stability.launch_count";

// Number of times the application exited uncleanly since the last report.
const char kStabilityCrashCount[] =
    "user_experience_metrics.stability.crash_count";

// Number of times the session end did not complete.
const char kStabilityIncompleteSessionEndCount[] =
    "user_experience_metrics.stability.incomplete_session_end_count";

// Number of times a page load event occurred since the last report.
const char kStabilityPageLoadCount[] =
    "user_experience_metrics.stability.page_load_count";

// Number of times a renderer process crashed since the last report.
const char kStabilityRendererCrashCount[] =
    "user_experience_metrics.stability.renderer_crash_count";

// Number of times an extension renderer process crashed since the last report.
const char kStabilityExtensionRendererCrashCount[] =
    "user_experience_metrics.stability.extension_renderer_crash_count";

// Time when the app was last launched, in seconds since the epoch.
const char kStabilityLaunchTimeSec[] =
    "user_experience_metrics.stability.launch_time_sec";

// Time when the app was last known to be running, in seconds since
// the epoch.
const char kStabilityLastTimestampSec[] =
    "user_experience_metrics.stability.last_timestamp_sec";

// This is the location of a list of dictionaries of plugin stability stats.
const char kStabilityPluginStats[] =
    "user_experience_metrics.stability.plugin_stats2";

// Number of times the renderer has become non-responsive since the last
// report.
const char kStabilityRendererHangCount[] =
    "user_experience_metrics.stability.renderer_hang_count";

// Total number of child process crashes (other than renderer / extension
// renderer ones, and plugin children, which are counted separately) since the
// last report.
const char kStabilityChildProcessCrashCount[] =
    "user_experience_metrics.stability.child_process_crash_count";

// On Chrome OS, total number of non-Chrome user process crashes
// since the last report.
const char kStabilityOtherUserCrashCount[] =
    "user_experience_metrics.stability.other_user_crash_count";

// On Chrome OS, total number of kernel crashes since the last report.
const char kStabilityKernelCrashCount[] =
    "user_experience_metrics.stability.kernel_crash_count";

// On Chrome OS, total number of unclean system shutdowns since the
// last report.
const char kStabilitySystemUncleanShutdownCount[] =
    "user_experience_metrics.stability.system_unclean_shutdowns";

// Number of times the browser has been able to register crash reporting.
const char kStabilityBreakpadRegistrationSuccess[] =
    "user_experience_metrics.stability.breakpad_registration_ok";

// Number of times the browser has failed to register crash reporting.
const char kStabilityBreakpadRegistrationFail[] =
    "user_experience_metrics.stability.breakpad_registration_fail";

// Number of times the browser has been run under a debugger.
const char kStabilityDebuggerPresent[] =
    "user_experience_metrics.stability.debugger_present";

// Number of times the browser has not been run under a debugger.
const char kStabilityDebuggerNotPresent[] =
    "user_experience_metrics.stability.debugger_not_present";

// The keys below are used for the dictionaries in the
// kStabilityPluginStats list.
const char kStabilityPluginName[] = "name";
const char kStabilityPluginLaunches[] = "launches";
const char kStabilityPluginInstances[] = "instances";
const char kStabilityPluginCrashes[] = "crashes";

// The keys below are strictly increasing counters over the lifetime of
// a chrome installation. They are (optionally) sent up to the uninstall
// survey in the event of uninstallation.
const char kUninstallMetricsPageLoadCount[] =
    "uninstall_metrics.page_load_count";
const char kUninstallLaunchCount[] = "uninstall_metrics.launch_count";
const char kUninstallMetricsInstallDate[] =
    "uninstall_metrics.installation_date2";
const char kUninstallMetricsUptimeSec[] = "uninstall_metrics.uptime_sec";
const char kUninstallLastLaunchTimeSec[] =
    "uninstall_metrics.last_launch_time_sec";
const char kUninstallLastObservedRunTimeSec[] =
    "uninstall_metrics.last_observed_running_time_sec";

// A collection of position, size, and other data relating to the browser
// window to restore on startup.
const char kBrowserWindowPlacement[] = "browser.window_placement";

// A collection of position, size, and other data relating to the task
// manager window to restore on startup.
const char kTaskManagerWindowPlacement[] = "task_manager.window_placement";

// A collection of position, size, and other data relating to the keyword
// editor window to restore on startup.
const char kKeywordEditorWindowPlacement[] = "keyword_editor.window_placement";

// A collection of position, size, and other data relating to the preferences
// window to restore on startup.
const char kPreferencesWindowPlacement[] = "preferences.window_placement";

// An integer specifying the total number of bytes to be used by the
// renderer's in-memory cache of objects.
const char kMemoryCacheSize[] = "renderer.memory_cache.size";

// String which specifies where to download files to by default.
const char kDownloadDefaultDirectory[] = "download.default_directory";

// Boolean that records if the download directory was changed by an
// upgrade a unsafe location to a safe location.
const char kDownloadDirUpgraded[] = "download.directory_upgrade";

// String which specifies where to save html files to by default.
const char kSaveFileDefaultDirectory[] = "savefile.default_directory";

// String which specifies the last directory that was chosen for uploading
// or opening a file.
const char kSelectFileLastDirectory[] = "selectfile.last_directory";

// Extensions which should be opened upon completion.
const char kDownloadExtensionsToOpen[] = "download.extensions_to_open";

// Integer which specifies the frequency in milliseconds for detecting whether
// plugin windows are hung.
const char kHungPluginDetectFrequency[] = "browser.hung_plugin_detect_freq";

// Integer which specifies the timeout value to be used for SendMessageTimeout
// to detect a hung plugin window.
const char kPluginMessageResponseTimeout[] =
    "browser.plugin_message_response_timeout";

// String which represents the dictionary name for our spell-checker.
const char kSpellCheckDictionary[] = "spellcheck.dictionary";

// Dictionary of schemes used by the external protocol handler.
// The value is true if the scheme must be ignored.
const char kExcludedSchemes[] = "protocol_handler.excluded_schemes";

// Keys used for MAC handling of SafeBrowsing requests.
const char kSafeBrowsingClientKey[] = "safe_browsing.client_key";
const char kSafeBrowsingWrappedKey[] = "safe_browsing.wrapped_key";

// Integer that specifies the index of the tab the user was on when they
// last visited the options window.
const char kOptionsWindowLastTabIndex[] = "options_window.last_tab_index";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";

// Integer that specifies the index of the tab the user was on when they
// last visited the Certificate Manager window.
const char kCertificateManagerWindowLastTabIndex[] =
    "certificate_manager_window.last_tab_index";

// The mere fact that this pref is registered signals that we should show the
// First Run Search Information bubble when the first browser window appears.
// This preference is only registered by the first-run procedure.
const char kShouldShowFirstRunBubble[] = "show-first-run-bubble";

// The mere fact that this pref is registered signals that we should show the
// smaller OEM First Run Search Information bubble when the first
// browser window appears.
// This preference is only registered by the first-run procedure.
const char kShouldUseOEMFirstRunBubble[] = "show-OEM-first-run-bubble";

// The mere fact that this pref is registered signals that we should show the
// minimal First Run omnibox information bubble when the first
// browser window appears.
// This preference is only registered by the first-run procedure.
const char kShouldUseMinimalFirstRunBubble[] = "show-minimal-first-run-bubble";

// Signal that we should show the welcome page when we launch Chrome.
const char kShouldShowWelcomePage[] = "show-welcome-page";

// String containing the last known Google URL.  We re-detect this on startup in
// most cases, and use it to send traffic to the correct Google host or with the
// correct Google domain/country code for whatever location the user is in.
const char kLastKnownGoogleURL[] = "browser.last_known_google_url";

// String containing the last prompted Google URL to the user.
// If the user is using .x TLD for Google URL and gets prompted about .y TLD
// for Google URL, and says "no", we should leave the search engine set to .x
// but not prompt again until the domain changes away from .y.
const char kLastPromptedGoogleURL[] = "browser.last_prompted_google_url";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
const char kLastKnownIntranetRedirectOrigin[] = "browser.last_redirect_origin";

// Integer containing the system Country ID the first time we checked the
// template URL prepopulate data.  This is used to avoid adding a whole bunch of
// new search engine choices if prepopulation runs when the user's Country ID
// differs from their previous Country ID.  This pref does not exist until
// prepopulation has been run at least once.
const char kCountryIDAtInstall[] = "countryid_at_install";
// OBSOLETE. Same as above, but uses the Windows-specific GeoID value instead.
// Updated if found to the above key.
const char kGeoIDAtInstall[] = "geoid_at_install";

// An enum value of how the browser was shut down (see browser_shutdown.h).
const char kShutdownType[] = "shutdown.type";
// Number of processes that were open when the user shut down.
const char kShutdownNumProcesses[] = "shutdown.num_processes";
// Number of processes that were shut down using the slow path.
const char kShutdownNumProcessesSlow[] = "shutdown.num_processes_slow";

// Whether to restart the current Chrome session automatically as the last thing
// before shutting everything down.
const char kRestartLastSessionOnShutdown[] = "restart.last.session.on.shutdown";

// Number of bookmarks/folders on the bookmark bar/other bookmark folder.
const char kNumBookmarksOnBookmarkBar[] =
    "user_experience_metrics.num_bookmarks_on_bookmark_bar";
const char kNumFoldersOnBookmarkBar[] =
    "user_experience_metrics.num_folders_on_bookmark_bar";
const char kNumBookmarksInOtherBookmarkFolder[] =
    "user_experience_metrics.num_bookmarks_in_other_bookmark_folder";
const char kNumFoldersInOtherBookmarkFolder[] =
    "user_experience_metrics.num_folders_in_other_bookmark_folder";

// Number of keywords.
const char kNumKeywords[] = "user_experience_metrics.num_keywords";

// Placeholder preference for disabling voice / video chat if it is ever added.
// Currently, this does not change any behavior.
const char kDisableVideoAndChat[] = "disable_video_chat";

// Whether Extensions are enabled.
const char kDisableExtensions[] = "extensions.disabled";

// Integer boolean representing the width (in pixels) of the container for
// browser actions.
const char kBrowserActionContainerWidth[] =
    "extensions.browseractions.container.width";

// A whitelist of extension ids the user can install: exceptions from the
// following blacklist. This is controlled by the administrator.
const char kExtensionInstallAllowList[] = "extensions.install.allowlist";
// A blacklist, containing extensions the user cannot install. This list can
// conatin "*" meaning all extensions. This is controlled by the administrator.
// This list should not be confused with the extension blacklist, which is
// Google controlled.
const char kExtensionInstallDenyList[] = "extensions.install.denylist";

// A list containing extensions that Chrome will silently install
// at startup time. It is a list of strings, each string contains
// an extension ID and an update URL, delimited by a semicolon.
// This preference is set by an admin policy, and meant to be only
// accessed through ExternalPolicyExtensionProvider.
const char kExtensionInstallForceList[] = "extensions.install.forcelist";

// Time of the last, and next scheduled, extensions auto-update checks.
const char kLastExtensionsUpdateCheck[] = "extensions.autoupdate.last_check";
const char kNextExtensionsUpdateCheck[] = "extensions.autoupdate.next_check";
// Version number of last blacklist check
const char kExtensionBlacklistUpdateVersion[] =
    "extensions.blacklistupdate.version";

const char kExtensionSidebarWidth[] = "extensions.sidebar.width";

// New Tab Page URLs that should not be shown as most visited thumbnails.
const char kNTPMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

// The URLs that have been pinned to the Most Visited section of the New Tab
// Page.
const char kNTPMostVisitedPinnedURLs[] = "ntp.pinned_urls";

// Data downloaded from resource pages (JSON, RSS) to be used to dynamically
// deliver data for the new tab page.
const char kNTPWebResourceCache[] = "ntp.web_resource_cache";

// Last time of update of web_resource_cache.
const char kNTPWebResourceCacheUpdate[] = "ntp.web_resource_cache_update";

// Serves resources for the NTP.
const char kNTPWebResourceServer[] = "ntp.web_resource_server";

// Serves tips for the NTP.
const char kNTPTipsResourceServer[] = "ntp.tips_resource_server";

// Serves dates to determine display of elements on the NTP.
const char kNTPDateResourceServer[] = "ntp.date_resource_server";

// Which sections should be visible on the new tab page
// 1 - Show the most visited sites in a grid
// 2 - Show the most visited sites as a list
// 4 - Show the recent section
// 8 - (Show tips -- DEPRECATED)
// 16 - Show sync status
const char kNTPShownSections[] = "ntp.shown_sections";

// This pref is used for migrating the prefs for the NTP
const char kNTPPrefVersion[] = "ntp.pref_version";

// Dates between which the NTP should show a custom logo rather than the
// standard one.
const char kNTPCustomLogoStart[] = "ntp.alt_logo_start";
const char kNTPCustomLogoEnd[] = "ntp.alt_logo_end";

// Whether promo should be shown to Dev builds, Beta and Dev, or all builds.
const char kNTPPromoBuild[] = "ntp.promo_build";

// True if user has explicitly closed the promo line.
const char kNTPPromoClosed[] = "ntp.promo_closed";

// Users are randomly divided into 16 groups in order to slowly roll out
// special promos.
const char kNTPPromoGroup[] = "ntp.promo_group";

// Amount of time each promo group should be shown a promo that is being slowly
// rolled out, in hours.
const char kNTPPromoGroupTimeSlice[] = "ntp.promo_group_timeslice";

// Promo line from server.
const char kNTPPromoLine[] = "ntp.promo_line";

// Dates between which the NTP should show a promotional line downloaded
// from the promo server.
const char kNTPPromoStart[] = "ntp.promo_start";
const char kNTPPromoEnd[] = "ntp.promo_end";

const char kDevToolsDisabled[] = "devtools.disabled";

// A boolean specifying whether dev tools window should be opened docked.
const char kDevToolsOpenDocked[] = "devtools.open_docked";

// Integer location of the split bar in the browser view.
const char kDevToolsSplitLocation[] = "devtools.split_location";

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// Boolean specifying whether the user finished setting up sync.
const char kSyncHasSetupCompleted[] = "sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const char kKeepEverythingSynced[] = "sync.keep_everything_synced";

// Booleans specifying whether the user has selected to sync the following
// datatypes.
const char kSyncBookmarks[] = "sync.bookmarks";
const char kSyncPasswords[] = "sync.passwords";
const char kSyncPreferences[] = "sync.preferences";
const char kSyncApps[] = "sync.apps";
const char kSyncAutofill[] = "sync.autofill";
const char kSyncAutofillProfile[] = "sync.autofill_profile";
const char kSyncThemes[] = "sync.themes";
const char kSyncTypedUrls[] = "sync.typed_urls";
const char kSyncExtensions[] = "sync.extensions";
const char kSyncSessions[] = "sync.sessions";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const char kSyncManaged[] = "sync.managed";

// Boolean to prevent sync from automatically starting up.  This is
// used when sync is disabled by the user via the privacy dashboard.
const char kSyncSuppressStart[] = "sync.suppress_start";

// Boolean to reperesent if sync credentials have been migrated from the
// user settings DB to the token service.
const char kSyncCredentialsMigrated[] = "sync.credentials_migrated";

// Boolean to represent whether the legacy autofill profile data has been
// migrated to the new model.
const char kAutofillProfileMigrated[] = "sync.autofill_migrated";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
const char kEncryptionBootstrapToken[] = "sync.encryption_bootstrap_token";

// Boolean tracking whether the user chose to specify a secondary encryption
// passphrase.
const char kSyncUsingSecondaryPassphrase[] = "sync.using_secondary_passphrase";

// String that identifies the user logged into sync and other google services.
const char kGoogleServicesUsername[] = "google.services.username";

// Create web application shortcut dialog preferences.
const char kWebAppCreateOnDesktop[] = "browser.web_app.create_on_desktop";
const char kWebAppCreateInAppsMenu[] = "browser.web_app.create_in_apps_menu";
const char kWebAppCreateInQuickLaunchBar[] =
    "browser.web_app.create_in_quick_launch_bar";

// Dictionary that maps Geolocation network provider server URLs to
// corresponding access token.
const char kGeolocationAccessToken[] = "geolocation.access_token";

// Whether PasswordForms have been migrated from the WedDataService to the
// LoginDatabase.
const char kLoginDatabaseMigrated[] = "login_database.migrated";

// The root URL of the cloud print service.
const char kCloudPrintServiceURL[] = "cloud_print.service_url";

// The last requested size of the dialog as it was closed.
const char kCloudPrintDialogWidth[] = "cloud_print.dialog_size.width";
const char kCloudPrintDialogHeight[] = "cloud_print.dialog_size.height";

const char kRemotingHasSetupCompleted[] = "remoting.has_setup_completed";

// The list of BackgroundContents that should be loaded when the browser
// launches.
const char kRegisteredBackgroundContents[] = "background_contents.registered";

// String that lists supported HTTP authentication schemes.
const char kAuthSchemes[] = "auth.schemes";

// Boolean that specifies whether to disable CNAME lookups when generating
// Kerberos SPN.
const char kDisableAuthNegotiateCnameLookup[] =
    "auth.disable_negotiate_cname_lookup";
// Boolean that specifies whether to include the port in a generated Kerberos
// SPN.
const char kEnableAuthNegotiatePort[] = "auth.enable_negotiate_port";
// Whitelist containing servers for which Integrated Authentication is enabled.
const char kAuthServerWhitelist[] = "auth.server_whitelist";
// Whitelist containing servers Chrome is allowed to do Kerberos delegation
// with.
const char kAuthNegotiateDelegateWhitelist[] =
    "auth.negotiate_delegate_whitelist";
// String that specifies the name of a custom GSSAPI library to load.
const char kGSSAPILibraryName[] = "auth.gssapi_library_name";

#if defined(OS_CHROMEOS)
// Dictionary for transient storage of settings that should go into signed
// settings storage before owner has been assigned.
const char kSignedSettingsTempStorage[] = "signed_settings_temp_storage";
#endif

// *************** SERVICE PREFS ***************
// These are attached to the service process.

const char kCloudPrintProxyEnabled[] = "cloud_print.enabled";
// The unique id for this instance of the cloud print proxy.
const char kCloudPrintProxyId[] = "cloud_print.proxy_id";
// The GAIA auth token for Cloud Print
const char kCloudPrintAuthToken[] = "cloud_print.auth_token";
// The GAIA auth token used by Cloud Print to authenticate with the XMPP server
// This should eventually go away because the above token should work for both.
const char kCloudPrintXMPPAuthToken[] = "cloud_print.xmpp_auth_token";
// The email address of the account used to authenticate with the Cloud Print
// server.
const char kCloudPrintEmail[] = "cloud_print.email";
// Settings specific to underlying print system.
const char kCloudPrintPrintSystemSettings[] =
    "cloud_print.print_system_settings";

// Used by the service process to determine if the remoting host is enabled.
const char kRemotingHostEnabled[] = "remoting.host_enabled";

// Integer to specify the type of proxy settings.
// See ProxyPrefs for possible values and interactions with the other proxy
// preferences.
const char kProxyMode[] = "proxy.mode";
// String specifying the proxy server. For a specification of the expected
// syntax see net::ProxyConfig::ProxyRules::ParseFromString().
const char kProxyServer[] = "proxy.server";
// URL to the proxy .pac file.
const char kProxyPacUrl[] = "proxy.pac_url";
// String containing proxy bypass rules. For a specification of the
// expected syntax see net::ProxyBypassRules::ParseFromString().
const char kProxyBypassList[] = "proxy.bypass_list";

// Preferences that are exclusivly used to store managed values for default
// content settings.
const char kManagedDefaultCookiesSetting[] =
    "profile.managed_default_content_settings.cookies";
const char kManagedDefaultImagesSetting[] =
    "profile.managed_default_content_settings.images";
const char kManagedDefaultJavaScriptSetting[] =
    "profile.managed_default_content_settings.javascript";
const char kManagedDefaultPluginsSetting[] =
    "profile.managed_default_content_settings.plugins";
const char kManagedDefaultPopupsSetting[] =
    "profile.managed_default_content_settings.popups";

// Dictionary for storing the set of known background pages (keys are extension
// IDs of background page owners, value is a boolean that is true if the user
// needs to acknowledge this page.
const char kKnownBackgroundPages[] = "background_pages.known";
}  // namespace prefs
