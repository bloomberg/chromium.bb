// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

#include "base/basictypes.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// A string property indicating whether default apps should be installed
// in this profile.  Use the value "install" to enable defaults apps, or
// "noinstall" to disable them.  This property is usually set in the
// master_preferences and copied into the profile preferences on first run.
// Defaults apps are installed only when creating a new profile.
const char kDefaultApps[] = "default_apps";

// Whether we have installed default apps yet in this profile.
const char kDefaultAppsInstalled[] = "default_apps_installed";

// A boolean specifying whether the New Tab page is the home page or not.
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const char kHomePage[] = "homepage";

// Did the user change the home page after install?
const char kHomePageChanged[] = "homepage_changed";

// Does this user have a Google+ Profile?
const char kIsGooglePlusUser[] = "is_google_plus_user";

// Used to determine if the last session exited cleanly. Set to false when
// first opened, and to true when closing. On startup if the value is false,
// it means the profile didn't exit cleanly.
const char kSessionExitedCleanly[] = "profile.exited_cleanly";

// An integer pref. Holds one of several values:
// 0: (deprecated) open the homepage on startup.
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
// 5: open the New Tab Page on startup.
const char kRestoreOnStartup[] = "session.restore_on_startup";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
const char kURLsToRestoreOnStartup[] = "session.urls_to_restore_on_startup";

// A preference to keep track of whether we have already checked whether we
// need to migrate the user from kRestoreOnStartup=0 to kRestoreOnStartup=4.
// We only need to do this check once, on upgrade from m18 or lower to m19 or
// higher.
const char kRestoreOnStartupMigrated[] = "session.restore_on_startup_migrated";

// Disables screenshot accelerators and extension APIs.
// This setting resides both in profile prefs and local state. Accelerator
// handling code reads local state, while extension APIs use profile pref.
const char kDisableScreenshots[] = "disable_screenshots";

// The application locale.
// For OS_CHROMEOS we maintain kApplicationLocale property in both local state
// and user's profile.  Global property determines locale of login screen,
// while user's profile determines his personal locale preference.
const char kApplicationLocale[] = "intl.app_locale";
#if defined(OS_CHROMEOS)
// Locale preference of device' owner.  ChromeOS device appears in this locale
// after startup/wakeup/signout.
const char kOwnerLocale[] = "intl.owner_locale";
// Locale accepted by user.  Non-syncable.
// Used to determine whether we need to show Locale Change notification.
const char kApplicationLocaleAccepted[] = "intl.app_locale_accepted";
// Non-syncable item.
// It is used in two distinct ways.
// (1) Used for two-step initialization of locale in ChromeOS
//     because synchronization of kApplicationLocale is not instant.
// (2) Used to detect locale change.  Locale change is detected by
//     LocaleChangeGuard in case values of kApplicationLocaleBackup and
//     kApplicationLocale are both non-empty and differ.
// Following is a table showing how state of those prefs may change upon
// common real-life use cases:
//                                  AppLocale Backup Accepted
// Initial login                       -        A       -
// Sync                                B        A       -
// Accept (B)                          B        B       B
// -----------------------------------------------------------
// Initial login                       -        A       -
// No sync and second login            A        A       -
// Change options                      B        B       -
// -----------------------------------------------------------
// Initial login                       -        A       -
// Sync                                A        A       -
// Locale changed on login screen      A        C       -
// Accept (A)                          A        A       A
// -----------------------------------------------------------
// Initial login                       -        A       -
// Sync                                B        A       -
// Revert                              A        A       -
const char kApplicationLocaleBackup[] = "intl.app_locale_backup";
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

// Obselete WebKit prefs for migration.
const char kGlobalDefaultCharset[] = "intl.global.charset_default";
const char kWebKitGlobalDefaultFontSize[] =
    "webkit.webprefs.global.default_font_size";
const char kWebKitGlobalDefaultFixedFontSize[] =
    "webkit.webprefs.global.default_fixed_font_size";
const char kWebKitGlobalMinimumFontSize[] =
    "webkit.webprefs.global.minimum_font_size";
const char kWebKitGlobalMinimumLogicalFontSize[] =
    "webkit.webprefs.global.minimum_logical_font_size";
const char kWebKitGlobalJavascriptCanOpenWindowsAutomatically[] =
    "webkit.webprefs.global.javascript_can_open_windows_automatically";
const char kWebKitGlobalJavascriptEnabled[] =
    "webkit.webprefs.global.javascript_enabled";
const char kWebKitGlobalLoadsImagesAutomatically[] =
    "webkit.webprefs.global.loads_images_automatically";
const char kWebKitGlobalPluginsEnabled[] =
    "webkit.webprefs.global.plugins_enabled";
const char kWebKitGlobalStandardFontFamily[] =
    "webkit.webprefs.global.standard_font_family";
const char kWebKitGlobalFixedFontFamily[] =
    "webkit.webprefs.global.fixed_font_family";
const char kWebKitGlobalSerifFontFamily[] =
    "webkit.webprefs.global.serif_font_family";
const char kWebKitGlobalSansSerifFontFamily[] =
    "webkit.webprefs.global.sansserif_font_family";
const char kWebKitGlobalCursiveFontFamily[] =
    "webkit.webprefs.global.cursive_font_family";
const char kWebKitGlobalFantasyFontFamily[] =
    "webkit.webprefs.global.fantasy_font_family";
const char kWebKitOldStandardFontFamily[] =
    "webkit.webprefs.standard_font_family";
const char kWebKitOldFixedFontFamily[] = "webkit.webprefs.fixed_font_family";
const char kWebKitOldSerifFontFamily[] = "webkit.webprefs.serif_font_family";
const char kWebKitOldSansSerifFontFamily[] =
    "webkit.webprefs.sansserif_font_family";
const char kWebKitOldCursiveFontFamily[] =
    "webkit.webprefs.cursive_font_family";
const char kWebKitOldFantasyFontFamily[] =
    "webkit.webprefs.fantasy_font_family";
const char kWebKitStandardFontFamilyMap[] =
    "webkit.webprefs.fonts.standard";
const char kWebKitFixedFontFamilyMap[] =
    "webkit.webprefs.fonts.fixed";
const char kWebKitSerifFontFamilyMap[] =
    "webkit.webprefs.fonts.serif";
const char kWebKitSansSerifFontFamilyMap[] =
    "webkit.webprefs.fonts.sansserif";
const char kWebKitCursiveFontFamilyMap[] =
    "webkit.webprefs.fonts.cursive";
const char kWebKitFantasyFontFamilyMap[] =
    "webkit.webprefs.fonts.fantasy";

// If these change, the corresponding enums in the extension API
// experimental.fontSettings.json must also change.
const char* const kWebKitScriptsForFontFamilyMaps[] = {
  "Afak", "Arab", "Armi", "Armn", "Avst", "Bali", "Bamu", "Bass", "Batk",
  "Beng", "Blis", "Bopo", "Brah", "Brai", "Bugi", "Buhd", "Cakm", "Cans",
  "Cari", "Cham", "Cher", "Cirt", "Copt", "Cprt", "Cyrl", "Cyrs", "Deva",
  "Dsrt", "Dupl", "Egyd", "Egyh", "Egyp", "Elba", "Ethi", "Geor", "Geok",
  "Glag", "Goth", "Gran", "Grek", "Gujr", "Guru", "Hang", "Hani", "Hano",
  "Hans", "Hant", "Hebr", "Hluw", "Hmng", "Hung", "Inds", "Ital", "Java",
  "Jpan", "Jurc", "Kali", "Khar", "Khmr", "Khoj", "Knda", "Kpel", "Kthi",
  "Lana", "Laoo", "Latf", "Latg", "Latn", "Lepc", "Limb", "Lina", "Linb",
  "Lisu", "Loma", "Lyci", "Lydi", "Mand", "Mani", "Maya", "Mend", "Merc",
  "Mero", "Mlym", "Moon", "Mong", "Mroo", "Mtei", "Mymr", "Narb", "Nbat",
  "Nkgb", "Nkoo", "Nshu", "Ogam", "Olck", "Orkh", "Orya", "Osma", "Palm",
  "Perm", "Phag", "Phli", "Phlp", "Phlv", "Phnx", "Plrd", "Prti", "Rjng",
  "Roro", "Runr", "Samr", "Sara", "Sarb", "Saur", "Sgnw", "Shaw", "Shrd",
  "Sind", "Sinh", "Sora", "Sund", "Sylo", "Syrc", "Syre", "Syrj", "Syrn",
  "Tagb", "Takr", "Tale", "Talu", "Taml", "Tang", "Tavt", "Telu", "Teng",
  "Tfng", "Tglg", "Thaa", "Thai", "Tibt", "Tirh", "Ugar", "Vaii", "Visp",
  "Wara", "Wole", "Xpeo", "Xsux", "Yiii", "Zmth", "Zsym", "Zyyy"
};

const size_t kWebKitScriptsForFontFamilyMapsLength =
    arraysize(kWebKitScriptsForFontFamilyMaps);

// WebKit preferences.
const char kWebKitStandardFontFamilyArabic[] =
    "webkit.webprefs.fonts.standard.Arab";
const char kWebKitFixedFontFamilyArabic[] =
    "webkit.webprefs.fonts.fixed.Arab";
const char kWebKitSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.serif.Arab";
const char kWebKitSansSerifFontFamilyArabic[] =
    "webkit.webprefs.fonts.sansserif.Arab";
const char kWebKitStandardFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.standard.Cyrl";
const char kWebKitFixedFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.fixed.Cyrl";
const char kWebKitSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.serif.Cyrl";
const char kWebKitSansSerifFontFamilyCyrillic[] =
    "webkit.webprefs.fonts.sansserif.Cyrl";
const char kWebKitStandardFontFamilyGreek[] =
    "webkit.webprefs.fonts.standard.Grek";
const char kWebKitFixedFontFamilyGreek[] =
    "webkit.webprefs.fonts.fixed.Grek";
const char kWebKitSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.serif.Grek";
const char kWebKitSansSerifFontFamilyGreek[] =
    "webkit.webprefs.fonts.sansserif.Grek";
const char kWebKitStandardFontFamilyJapanese[] =
    "webkit.webprefs.fonts.standard.Jpan";
const char kWebKitFixedFontFamilyJapanese[] =
    "webkit.webprefs.fonts.fixed.Jpan";
const char kWebKitSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.serif.Jpan";
const char kWebKitSansSerifFontFamilyJapanese[] =
    "webkit.webprefs.fonts.sansserif.Jpan";
const char kWebKitStandardFontFamilyKorean[] =
    "webkit.webprefs.fonts.standard.Hang";
const char kWebKitFixedFontFamilyKorean[] =
    "webkit.webprefs.fonts.fixed.Hang";
const char kWebKitSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.serif.Hang";
const char kWebKitSansSerifFontFamilyKorean[] =
    "webkit.webprefs.fonts.sansserif.Hang";
const char kWebKitCursiveFontFamilyKorean[] =
    "webkit.webprefs.fonts.cursive.Hang";
const char kWebKitStandardFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.standard.Hans";
const char kWebKitFixedFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.fixed.Hans";
const char kWebKitSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.serif.Hans";
const char kWebKitSansSerifFontFamilySimplifiedHan[] =
    "webkit.webprefs.fonts.sansserif.Hans";
const char kWebKitStandardFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.standard.Hant";
const char kWebKitFixedFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.fixed.Hant";
const char kWebKitSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.serif.Hant";
const char kWebKitSansSerifFontFamilyTraditionalHan[] =
    "webkit.webprefs.fonts.sansserif.Hant";

const char kWebKitWebSecurityEnabled[] = "webkit.webprefs.web_security_enabled";
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
const char kWebKitAllowDisplayingInsecureContent[] =
    "webkit.webprefs.allow_displaying_insecure_content";
const char kWebKitAllowRunningInsecureContent[] =
    "webkit.webprefs.allow_running_insecure_content";

const char kWebKitCommonScript[] = "Zyyy";
const char kWebKitStandardFontFamily[] = "webkit.webprefs.fonts.standard.Zyyy";
const char kWebKitFixedFontFamily[] = "webkit.webprefs.fonts.fixed.Zyyy";
const char kWebKitSerifFontFamily[] = "webkit.webprefs.fonts.serif.Zyyy";
const char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.fonts.sansserif.Zyyy";
const char kWebKitCursiveFontFamily[] = "webkit.webprefs.fonts.cursive.Zyyy";
const char kWebKitFantasyFontFamily[] = "webkit.webprefs.fonts.fantasy.Zyyy";
const char kWebKitDefaultFontSize[] = "webkit.webprefs.default_font_size";
const char kWebKitDefaultFixedFontSize[] =
    "webkit.webprefs.default_fixed_font_size";
const char kWebKitMinimumFontSize[] = "webkit.webprefs.minimum_font_size";
const char kWebKitMinimumLogicalFontSize[] =
    "webkit.webprefs.minimum_logical_font_size";
const char kWebKitJavascriptEnabled[] = "webkit.webprefs.javascript_enabled";
const char kWebKitJavascriptCanOpenWindowsAutomatically[] =
    "webkit.webprefs.javascript_can_open_windows_automatically";
const char kWebKitLoadsImagesAutomatically[] =
    "webkit.webprefs.loads_images_automatically";
const char kWebKitPluginsEnabled[] = "webkit.webprefs.plugins_enabled";

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";

// Boolean which specifies the ids of the bookmark nodes that are expanded in
// the bookmark editor.
const char kBookmarkEditorExpandedNodes[] = "bookmark_editor.expanded_nodes";

// Boolean that is true if the password manager is on (will record new
// passwords and fill in known passwords).
const char kPasswordManagerEnabled[] = "profile.password_manager_enabled";

// Boolean controlling whether the password manager allows to retrieve passwords
// in clear text.
const char kPasswordManagerAllowShowPasswords[] =
    "profile.password_manager_allow_show_passwords";

// Boolean that is true when password generation is enabled.
const char kPasswordGenerationEnabled[] = "password_generation.enabled";

// Booleans identifying whether normal and reverse auto-logins are enabled.
const char kAutologinEnabled[] = "autologin.enabled";
const char kReverseAutologinEnabled[] = "reverse_autologin.enabled";

// List to keep track of emails for which the user has rejected one-click
// sign-in.
const char kReverseAutologinRejectedEmailList[] =
    "reverse_autologin.rejected_email_list";

// Boolean that is true when SafeBrowsing is enabled.
const char kSafeBrowsingEnabled[] = "safebrowsing.enabled";

// Boolean that is true when SafeBrowsing Malware Report is enabled.
const char kSafeBrowsingReportingEnabled[] =
    "safebrowsing.reporting_enabled";

// Boolean that is true when the SafeBrowsing interstitial should not allow
// users to proceed anyway.
const char kSafeBrowsingProceedAnywayDisabled[] =
    "safebrowsing.proceed_anyway_disabled";

// Enum that specifies whether Incognito mode is:
// 0 - Enabled. Default behaviour. Default mode is available on demand.
// 1 - Disabled. Used cannot browse pages in Incognito mode.
// 2 - Forced. All pages/sessions are forced into Incognito.
const char kIncognitoModeAvailability[] = "incognito.mode_availability";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// Boolean that indicates whether the browser should put up a confirmation
// window when the user is attempting to quit. Mac only.
const char kConfirmToQuitEnabled[] = "browser.confirm_to_quit";

// OBSOLETE.  Enum that specifies whether to enforce a third-party cookie
// blocking policy.  This has been superseded by kDefaultContentSettings +
// kBlockThirdPartyCookies.
// 0 - allow all cookies.
// 1 - block third-party cookies
// 2 - block all cookies
const char kCookieBehavior[] = "security.cookie_behavior";

// The GUID of the synced default search provider. Note that this acts like a
// pointer to which synced search engine should be the default, rather than the
// prefs below which describe the locally saved default search provider details
// (and are not synced). This is ignored in the case of the default search
// provider being managed by policy.
const char kSyncedDefaultSearchProviderGUID[] =
    "default_search_provider.synced_guid";

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

// The Favicon URL (as understood by TemplateURLRef) of the default search
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

// Prefs for persisting HttpServerProperties.
const char kHttpServerProperties[] = "net.http_server_properties";

// Prefs for server names that support SPDY protocol.
const char kSpdyServers[] = "spdy.servers";

// Prefs for servers that support Alternate-Protocol.
const char kAlternateProtocolServers[] = "spdy.alternate_protocol";

// Disables the listed protocol schemes.
const char kDisabledSchemes[] = "protocol.disabled_schemes";

// Blocks access to the listed host patterns.
const char kUrlBlacklist[] = "policy.url_blacklist";

// Allows access to the listed host patterns, as exceptions to the blacklist.
const char kUrlWhitelist[] = "policy.url_whitelist";

// Double pref for a scaling factor used to slow down animations.
const char kInstantAnimationScaleFactor[] =
    "instant.animation_scale_factor";

// Boolean pref indicating whether the instant confirm dialog has been shown.
const char kInstantConfirmDialogShown[] = "instant.confirm_dialog_shown";

// Boolean pref indicating if instant is enabled.
const char kInstantEnabled[] = "instant.enabled";

// Used to migrate preferences from local state to user preferences to
// enable multiple profiles.
// BITMASK with possible values (see browser_prefs.cc for enum):
// 0: No preferences migrated.
// 1: DNS preferences migrated: kDnsPrefetchingStartupList and HostReferralList
// 2: Browser window preferences migrated: kDevToolsSplitLocation and
//    kBrowserWindowPlacement
const char kMultipleProfilePrefMigration[] =
    "local_state.multiple_profile_prefs_version";

// A boolean pref set to true if prediction of network actions is allowed.
// Actions include DNS prefetching, TCP and SSL preconnection, and prerendering
// of web pages.
// NOTE: The "dns_prefetching.enabled" value is used so that historical user
// preferences are not lost.
const char kNetworkPredictionEnabled[] = "dns_prefetching.enabled";

// An integer representing the state of the default apps installation process.
// This value is persisted in the profile's user preferences because the process
// is async, and the user may have stopped chrome in the middle.  The next time
// the profile is opened, the process will continue from where it left off.
//
// See possible values in external_provider_impl.cc.
const char kDefaultAppsInstallState[] = "default_apps_install_state";

#if defined(OS_CHROMEOS)
// An integer pref to initially mute volume if 1. This pref is ignored if
// |kAudioOutputAllowed| is set to false, but its value is preserved, therefore
// when the policy is lifted the original mute state is restored.
const char kAudioMute[] = "settings.audio.mute";

// TODO(derat): This is deprecated in favor of |kAudioVolumePercent|; remove it
// after R20 once we've cleared old user settings: http://crbug.com/112039
const char kAudioVolumeDb[] = "settings.audio.volume";

// A double pref storing the user-requested volume.
const char kAudioVolumePercent[] = "settings.audio.volume_percent";

// A boolean pref set to true if touchpad tap-to-click is enabled.
const char kTapToClickEnabled[] = "settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if touchpad three-finger-click is enabled.
const char kEnableTouchpadThreeFingerClick[] =
    "settings.touchpad.enable_three_finger_click";

// A boolean pref set to true if touchpad natural scrolling is enabled.
const char kNaturalScroll[] = "settings.touchpad.natural_scroll";

// A boolean pref set to true if primary mouse button is the left button.
const char kPrimaryMouseButtonRight[] = "settings.mouse.primary_right";

// A integer pref for the touchpad sensitivity.
const char kMouseSensitivity[] = "settings.mouse.sensitivity2";

// A integer pref for the touchpad sensitivity.
const char kTouchpadSensitivity[] = "settings.touchpad.sensitivity2";

// A boolean pref set to true if time should be displayed in 24-hour clock.
const char kUse24HourClock[] = "settings.clock.use_24hour_clock";

// A boolean pref to disable gdata.
const char kDisableGData[] = "gdata.disabled";

// A boolean pref to disable gdata over cellular connections.
const char kDisableGDataOverCellular[] = "gdata.cellular.disabled";

// A boolean pref to disable gdata hosted files.
const char kDisableGDataHostedFiles[] = "gdata.hosted_files.disabled";

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
const char kLanguageHangulHanjaBindingKeys[] =
    "settings.language.hangul_hanja_binding_keys";

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
const char kLanguageMozcUseHistorySuggest[] =
    "settings.language.mozc_use_history_suggest";
const char kLanguageMozcUseDictionarySuggest[] =
    "settings.language.mozc_use_dictionary_suggest";
const char kLanguageMozcSuggestionsSize[] =
    "settings.language.mozc_suggestions_size";

// A integer prefs which determine how we remap modifier keys (e.g. swap Alt-L
// and Control-L.) Possible values for these prefs are 0-4. See ModifierKey enum
// in src/chrome/browser/chromeos/input_method/xkeyboard.h
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

// A boolean pref which determines whether spoken feedback is enabled.
const char kSpokenFeedbackEnabled[] = "settings.accessibility";
// A boolean pref which determines whether high conrast is enabled.
const char kHighContrastEnabled[] = "settings.a11y.high_contrast_enabled";
// A boolean pref which determines whether screen magnifier is enabled.
const char kScreenMagnifierEnabled[] = "settings.a11y.screen_magnifier";
// A boolean pref which determines whether virtual keyboard is enabled.
// TODO(hashimoto): Remove this pref.
const char kVirtualKeyboardEnabled[] = "settings.a11y.virtual_keyboard";

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

// A boolean pref of whether to show 3G promo notification.
const char kShow3gPromoNotification[] =
    "settings.internet.mobile.show_3g_promo_notification";

// A string pref that contains version where "What's new" promo was shown.
const char kChromeOSReleaseNotesVersion[] = "settings.release_notes.version";

// A boolean pref that uses shared proxies.
const char kUseSharedProxies[] = "settings.use_shared_proxies";

// A string prefs for OAuth1 token.
const char kOAuth1Token[] = "settings.account.oauth1_token";

// A string prefs for OAuth1 secret.
const char kOAuth1Secret[] = "settings.account.oauth1_secret";

// A boolean pref that enables the (private) pepper GetID() call.
const char kEnableCrosDRM[] = "settings.privacy.drm_enabled";

// An enumeration that specifies the layout of the secondary display.
//  0 - The secondary display is at the top of the primary display.
//  1 - The secondary display is at the right of the primary display.
//  2 - The secondary display is at the bottom of the primary display.
//  3 - The secondary display is at the left of the primary display.
// TODO(mukai,oshima): update the format of the multi-display settings.
const char kSecondaryDisplayLayout[] = "settings.display.secondary_layout";

// An integer pref that specifies how far the secondary display is positioned
// from the edge of the primary display.
const char kSecondaryDisplayOffset[] = "settings.display.secondary_offset";

// A dictionary pref that specifies per-display layout/offset information.
// Its key is the ID of the display and its value is a dictionary for the
// layout/offset information.
const char kSecondaryDisplays[] = "settings.display.secondary_displays";
#endif  // defined(OS_CHROMEOS)

// The disabled messages in IPC logging.
const char kIpcDisabledMessages[] = "ipc_log_disabled_messages";

// A boolean pref set to true if a Home button to open the Home pages should be
// visible on the toolbar.
const char kShowHomeButton[] = "browser.show_home_button";

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
const char kDeleteHostedAppsData[] = "browser.clear_data.hosted_apps_data";
const char kDeauthorizeContentLicenses[] =
    "browser.clear_data.content_licenses";
const char kDeleteTimePeriod[] = "browser.clear_data.time_period";

// Boolean pref to define the default values for using spellchecker.
const char kEnableSpellCheck[] = "browser.enable_spellchecking";

// List of names of the enabled labs experiments (see chrome/browser/labs.cc).
const char kEnabledLabsExperiments[] = "browser.enabled_labs_experiments";

// Boolean pref to define the default values for using auto spell correct.
const char kEnableAutoSpellCorrect[] = "browser.enable_autospellcorrect";

// Boolean pref to define the default setting for "block offensive words".
// The old key value is kept to avoid unnecessary migration code.
const char kSpeechRecognitionFilterProfanities[] =
    "browser.speechinput_censor_results";

// List of speech recognition context names (extensions or websites) for which
// the tray notification balloon has already been shown.
const char kSpeechRecognitionTrayNotificationShownContexts[] =
    "browser.speechinput_tray_notification_shown_contexts";

// Boolean controlling whether history saving is disabled.
const char kSavingBrowserHistoryDisabled[] = "history.saving_disabled";

#if defined(TOOLKIT_GTK)
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

// Dictionary pref that tracks which command belongs to which
// extension + named command pair.
const char kExtensionCommands[] = "extensions.commands";

// Pref containing the directory for internal plugins as written to the plugins
// list (below).
const char kPluginsLastInternalDirectory[] = "plugins.last_internal_directory";

// List pref containing information (dictionaries) on plugins.
const char kPluginsPluginsList[] = "plugins.plugins_list";

// List pref containing names of plugins that are disabled by policy.
const char kPluginsDisabledPlugins[] = "plugins.plugins_disabled";

// List pref containing exceptions to the list of plugins disabled by policy.
const char kPluginsDisabledPluginsExceptions[] =
    "plugins.plugins_disabled_exceptions";

// List pref containing names of plugins that are enabled by policy.
const char kPluginsEnabledPlugins[] = "plugins.plugins_enabled";

// When first shipped, the pdf plugin will be disabled by default.  When we
// enable it by default, we'll want to do so only once.
const char kPluginsEnabledInternalPDF[] = "plugins.enabled_internal_pdf3";

// When first shipped, the nacl plugin will be disabled by default.  When we
// enable it by default, we'll want to do so only once.
const char kPluginsEnabledNaCl[] = "plugins.enabled_nacl";

// When bundled NPAPI Flash is removed, if at that point it is enabled while
// Pepper Flash is disabled, we would like to turn on Pepper Flash. And we will
// want to do so only once.
const char kPluginsMigratedToPepperFlash[] = "plugins.migrated_to_pepper_flash";

#if !defined(OS_ANDROID)
const char kPluginsShowSetReaderDefaultInfobar[] =
    "plugins.show_set_reader_default";

// Whether about:plugins is shown in the details mode or not.
const char kPluginsShowDetails[] = "plugins.show_details";
#endif

// Boolean that indicates whether outdated plugins are allowed or not.
const char kPluginsAllowOutdated[] = "plugins.allow_outdated";

// Boolean that indicates whether plugins that require authorization should
// be always allowed or not.
const char kPluginsAlwaysAuthorize[] = "plugins.always_authorize";

// Boolean that indicates whether we should check if we are the default browser
// on start-up.
const char kCheckDefaultBrowser[] = "browser.check_default_browser";

#if defined(OS_WIN)
// By default, setting Chrome as default during first run on Windows 8 will
// trigger shutting down the current instance and spawning a new (Metro)
// Chrome. This boolean preference supresses this behaviour.
const char kSuppressSwitchToMetroModeOnSetDefault[] =
    "browser.suppress_switch_to_metro_mode_on_set_default";
#endif

// Policy setting whether default browser check should be disabled and default
// browser registration should take place.
const char kDefaultBrowserSettingEnabled[] =
    "browser.default_browser_setting_enabled";

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

// Boolean indicating whether the clear on exit pref was migrated to content
// settings yet.
const char kContentSettingsClearOnExitMigrated[] =
    "profile.content_settings.clear_on_exit_migrated";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Patterns for mapping hostnames to content related settings. Default settings
// will be applied to hosts that don't match any of the patterns. Replaces
// kPerHostContentSettings. The pattern format used is defined by
// kContentSettingsVersion.
const char kContentSettingsPatterns[] = "profile.content_settings.patterns";

const char kContentSettingsPatternPairs[] =
    "profile.content_settings.pattern_pairs";

// Version of the content settings whitelist.
const char kContentSettingsDefaultWhitelistVersion[] =
    "profile.content_settings.whitelist_version";

#if !defined(OS_ANDROID)
// Which plugins have been whitelisted manually by the user.
const char kContentSettingsPluginWhitelist[] =
    "profile.content_settings.plugin_whitelist";
#endif

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const char kBlockThirdPartyCookies[] = "profile.block_third_party_cookies";

// Boolean that is true when all locally stored site data (e.g. cookies, local
// storage, etc..) should be deleted on exit.
const char kClearSiteDataOnExit[] = "profile.clear_site_data_on_exit";

// Double that indicates the default zoom level.
const char kDefaultZoomLevel[] = "profile.default_zoom_level";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const char kPerHostZoomLevels[] = "profile.per_host_zoom_levels";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillEnabled[] = "autofill.enabled";

// Boolean that is true when auxiliary Autofill profiles are enabled.
// Currently applies to Address Book "me" card on Mac.  False on Win and Linux.
const char kAutofillAuxiliaryProfilesEnabled[] =
    "autofill.auxiliary_profiles_enabled";

// Double that indicates positive (for matched forms) upload rate.
const char kAutofillPositiveUploadRate[] = "autofill.positive_upload_rate";

// Double that indicates negative (for not matched forms) upload rate.
const char kAutofillNegativeUploadRate[] = "autofill.negative_upload_rate";

// Boolean option set to true on the first run. Non-persistent.
const char kAutofillPersonalDataManagerFirstRun[] = "autofill.pdm.first_run";

// Modifying bookmarks is completely disabled when this is set to false.
const char kEditBookmarksEnabled[] = "bookmarks.editing_enabled";

// Boolean that is true when the translate feature is enabled.
const char kEnableTranslate[] = "translate.enabled";

#if !defined(OS_ANDROID)
const char kPinnedTabs[] = "pinned_tabs";
#endif

// Integer containing the default Geolocation content setting.
const char kGeolocationDefaultContentSetting[] =
    "geolocation.default_content_setting";

#if defined(OS_ANDROID)
// Boolean that controls the enabled-state of Geolocation.
const char kGeolocationEnabled[] = "geolocation.enabled";
#endif

// Dictionary that maps [frame, toplevel] to their Geolocation content setting.
const char kGeolocationContentSettings[] = "geolocation.content_settings";

// Preference to disable 3D APIs (WebGL, Pepper 3D).
const char kDisable3DAPIs[] = "disable_3d_apis";

// Whether to enable hyperlink auditing ("<a ping>").
const char kEnableHyperlinkAuditing[] = "enable_a_ping";

// Whether to enable sending referrers.
const char kEnableReferrers[] = "enable_referrers";

// Boolean to enable reporting memory info to page.
const char kEnableMemoryInfo[] = "enable_memory_info";

// Boolean that specifies whether to import bookmarks from the default browser
// on first run.
const char kImportBookmarks[] = "import_bookmarks";

// Boolean that specifies whether to import the browsing history from the
// default browser on first run.
const char kImportHistory[] = "import_history";

// Boolean that specifies whether to import the homepage from the default
// browser on first run.
const char kImportHomepage[] = "import_home_page";

// Boolean that specifies whether to import the search engine from the default
// browser on first run.
const char kImportSearchEngine[] = "import_search_engine";

// Boolean that specifies whether to import the saved passwords from the default
// browser on first run.
const char kImportSavedPasswords[] = "import_saved_passwords";

// The URL of the enterprise web store, which is a site trusted by the
// enterprise admin. Users can install apps & extensions from this site
// without scary warnings.
const char kEnterpriseWebStoreURL[] = "webstore.enterprise_store_url";

// The name of the enterprise web store, to be shown to the user.
const char kEnterpriseWebStoreName[] = "webstore.enterprise_store_name";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// The local profile id for this profile.
const char kLocalProfileId[] = "profile.local_profile_id";

// Whether passwords in external services (e.g. GNOME Keyring) have been tagged
// with the local profile id yet. (Used for migrating to tagged passwords.)
const char kPasswordsUseLocalProfileId[] =
    "profile.passwords_use_local_profile_id";
#endif

// Profile avatar and name
const char kProfileAvatarIndex[] = "profile.avatar_index";
const char kProfileName[] = "profile.name";

// Indicates if we've already shown a notification that high contrast
// mode is on, recommending high-contrast extensions and themes.
const char kInvertNotificationShown[] = "invert_notification_version_2_shown";

// Boolean controlling whether printing is enabled.
const char kPrintingEnabled[] = "printing.enabled";

// Boolean controlling whether print preview is disabled.
const char kPrintPreviewDisabled[] = "printing.print_preview_disabled";

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

// Directory of the last profile used.
const char kProfileLastUsed[] = "profile.last_used";

// List of directories of the profiles last active.
const char kProfilesLastActive[] = "profile.last_active_profiles";

// Total number of profiles created for this Chrome build. Used to tag profile
// directories.
const char kProfilesNumCreated[] = "profile.profiles_created";

// String containing the version of Chrome that the profile was created by.
// If profile was created before this feature was added, this pref will default
// to "1.0.0.0".
const char kProfileCreatedByVersion[] = "profile.created_by_version";

// A map of profile data directory to cached information. This cache can be
// used to display information about profiles without actually having to load
// them.
const char kProfileInfoCache[] = "profile.info_cache";

// Prefs for SSLConfigServicePref.
const char kCertRevocationCheckingEnabled[] = "ssl.rev_checking.enabled";
const char kSSLVersionMin[] = "ssl.version_min";
const char kSSLVersionMax[] = "ssl.version_max";
const char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";
const char kEnableOriginBoundCerts[] = "ssl.origin_bound_certs.enabled";
const char kDisableSSLRecordSplitting[] = "ssl.ssl_record_splitting.disabled";

// The metrics client GUID, entropy source and session ID.
const char kMetricsClientID[] = "user_experience_metrics.client_id";
const char kMetricsSessionID[] = "user_experience_metrics.session_id";
const char kMetricsLowEntropySource[] =
    "user_experience_metrics.low_entropy_source";

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
// info, etc.  Currently we store both XML and protobuf versions of these logs.
const char kMetricsInitialLogsXml[] =
    "user_experience_metrics.initial_logs";
const char kMetricsInitialLogsProto[] =
    "user_experience_metrics.initial_logs_as_protobufs";

// Array of strings that are each UMA logs that were not sent because the
// browser terminated before these accumulated metrics could be sent.  These
// logs typically include histograms and memory reports, as well as ongoing
// user activities.  Currently we store both XML and protobuf versions.
const char kMetricsOngoingLogsXml[] =
    "user_experience_metrics.ongoing_logs";
const char kMetricsOngoingLogsProto[] =
    "user_experience_metrics.ongoing_logs_as_protobufs";

// String serialized form of variations seed protobuf.
const char kVariationsSeed[] = "variations_seed";

// 64-bit integer serialization of the base::Time from the last seed received.
const char kVariationsSeedDate[] = "variations_seed_date";

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
const char kStabilityPluginLoadingErrors[] = "loading_errors";

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

// The type used to save the page. See the enum SavePackage::SavePackageType in
// the chrome/browser/download/save_package.h for the possible values.
const char kSaveFileType[] = "savefile.type";

// String which specifies the last directory that was chosen for uploading
// or opening a file.
const char kSelectFileLastDirectory[] = "selectfile.last_directory";

// Boolean that specifies if file selection dialogs are shown.
const char kAllowFileSelectionDialogs[] = "select_file_dialogs.allowed";

// Map of default tasks, associated by MIME type.
const char kDefaultTasksByMimeType[] =
    "filebrowser.tasks.default_by_mime_type";

// Map of default tasks, associated by file suffix.
const char kDefaultTasksBySuffix[] =
    "filebrowser.tasks.default_by_suffix";

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

// Boolean pref indicating whether the spelling confirm dialog has been shown.
const char kSpellCheckConfirmDialogShown[] = "spellcheck.confirm_dialog_shown";

// String which represents whether we use the spelling service.
const char kSpellCheckUseSpellingService[] = "spellcheck.use_spelling_service";

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

// Boolean that specifies if the first run bubble should be shown.
// This preference is only registered by the first-run procedure.
const char kShouldShowFirstRunBubble[] = "show-first-run-bubble";

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

// Set before autorestarting Chrome, cleared on clean exit.
const char kWasRestarted[] = "was.restarted";

#if defined(OS_WIN)
// On Windows 8 chrome can restart in desktop or in metro mode.
const char kRestartSwitchMode[] = "restart.switch_mode";
#endif

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

// Whether the plugin finder that lets you install missing plug-ins is enabled.
const char kDisablePluginFinder[] = "plugins.disable_plugin_finder";

// Integer boolean representing the width (in pixels) of the container for
// browser actions.
const char kBrowserActionContainerWidth[] =
    "extensions.browseractions.container.width";

// The sites that are allowed to install extensions. These sites should be
// allowed to install extensions without the scary dangerous downloads bar.
// Also, when off-store-extension installs are disabled, these sites are exempt.
const char kExtensionAllowedInstallSites[] = "extensions.allowed_install_sites";

// A whitelist of extension ids the user can install: exceptions from the
// following blacklist.
const char kExtensionInstallAllowList[] = "extensions.install.allowlist";

// A blacklist, containing extensions the user cannot install. This list can
// contain "*" meaning all extensions. This list should not be confused with the
// extension blacklist, which is Google controlled.
const char kExtensionInstallDenyList[] = "extensions.install.denylist";

// Whether we have run the extension-alert system (see ExtensionGlobalError)
// at least once for this profile.
const char kExtensionAlertsInitializedPref[] = "extensions.alerts.initialized";

// A list containing extensions that Chrome will silently install
// at startup time. It is a list of strings, each string contains
// an extension ID and an update URL, delimited by a semicolon.
// This preference is set by an admin policy, and meant to be only
// accessed through extensions::ExternalPolicyProvider.
const char kExtensionInstallForceList[] = "extensions.install.forcelist";

// Time of the last, and next scheduled, extensions auto-update checks.
const char kLastExtensionsUpdateCheck[] = "extensions.autoupdate.last_check";
const char kNextExtensionsUpdateCheck[] = "extensions.autoupdate.next_check";
// Version number of last blacklist check
const char kExtensionBlacklistUpdateVersion[] =
    "extensions.blacklistupdate.version";

// Keeps track of which sessions are collapsed in the Other Devices menu.
const char kNtpCollapsedForeignSessions[] = "ntp.collapsed_foreign_sessions";

// New Tab Page URLs that should not be shown as most visited thumbnails.
const char kNtpMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

// Data downloaded from promo resource pages (JSON, RSS) to be used to
// dynamically deliver data for the new tab page.
const char kNtpPromoResourceCache[] = "ntp.promo_resource_cache";

// Last time of update of promo_resource_cache.
const char kNtpPromoResourceCacheUpdate[] = "ntp.promo_resource_cache_update";

// Serves promo resources for the NTP.
const char kNtpPromoResourceServer[] = "ntp.web_resource_server";

// Serves tips for the NTP.
const char kNtpTipsResourceServer[] = "ntp.tips_resource_server";

// Serves dates to determine display of elements on the NTP.
const char kNtpDateResourceServer[] = "ntp.date_resource_server";

// Which bookmarks folder should be visible on the new tab page v4.
const char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";

// Which page should be visible on the new tab page v4
const char kNtpShownPage[] = "ntp.shown_page";

// TODO(achuith): Deprecated, will be removed (M23)
// Dates between which the NTP should show a custom logo rather than the
// standard one.
const char kNtpCustomLogoStart[] = "ntp.alt_logo_start";
const char kNtpCustomLogoEnd[] = "ntp.alt_logo_end";

// The promo resource service version number.
const char kNtpPromoVersion[] = "ntp.promo_version";

// The last locale the promo was fetched for.
const char kNtpPromoLocale[] = "ntp.promo_locale";

// True if a desktop sync session was found for this user.
const char kNtpPromoDesktopSessionFound[] = "ntp.promo_desktop_session_found";

// Boolean indicating whether the web store is active for the current locale.
const char kNtpWebStoreEnabled[] = "ntp.webstore_enabled";

// The id of the last web store promo actually displayed on the NTP.
const char kNtpWebStorePromoLastId[] = "ntp.webstore_last_promo_id";

// The id of the current web store promo.
const char kNtpWebStorePromoId[] = "ntp.webstorepromo.id";

// The header line for the NTP web store promo.
const char kNtpWebStorePromoHeader[] = "ntp.webstorepromo.header";

// The button text for the NTP web store promo.
const char kNtpWebStorePromoButton[] = "ntp.webstorepromo.button";

// The button link for the NTP web store promo.
const char kNtpWebStorePromoLink[] = "ntp.webstorepromo.link";

// The image URL for the NTP web store promo logo.
const char kNtpWebStorePromoLogo[] = "ntp.webstorepromo.logo";

// The original URL for the NTP web store promo logo.
const char kNtpWebStorePromoLogoSource[] = "ntp.webstorepromo.logo_source";

// The "hide this" link text for the NTP web store promo.
const char kNtpWebStorePromoExpire[] = "ntp.webstorepromo.expire";

// Specifies what users should maximize the NTP web store promo.
const char kNtpWebStorePromoUserGroup[] = "ntp.webstorepromo.usergroup";

// Customized app page names that appear on the New Tab Page.
const char kNtpAppPageNames[] = "ntp.app_page_names";

const char kDevToolsDisabled[] = "devtools.disabled";

// A boolean specifying whether dev tools window should be opened docked.
const char kDevToolsOpenDocked[] = "devtools.open_docked";

// A string specifying the dock location (either 'bottom' or 'right').
const char kDevToolsDockSide[] = "devtools.dock_side";

// Integer location of the horizontal split bar in the browser view.
const char kDevToolsHSplitLocation[] = "devtools.split_location";

// Integer location of the vertical split bar in the browser view.
const char kDevToolsVSplitLocation[] = "devtools.v_split_location";

// Maps of files edited locally using DevTools.
const char kDevToolsEditedFiles[] = "devtools.edited_files";

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// Boolean specifying whether the user finished setting up sync.
const char kSyncHasSetupCompleted[] = "sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const char kSyncKeepEverythingSynced[] = "sync.keep_everything_synced";

// Booleans specifying whether the user has selected to sync the following
// datatypes.
const char kSyncBookmarks[] = "sync.bookmarks";
const char kSyncPasswords[] = "sync.passwords";
const char kSyncPreferences[] = "sync.preferences";
const char kSyncAppNotifications[] = "sync.app_notifications";
const char kSyncAppSettings[] = "sync.app_settings";
const char kSyncApps[] = "sync.apps";
const char kSyncAutofill[] = "sync.autofill";
const char kSyncAutofillProfile[] = "sync.autofill_profile";
const char kSyncThemes[] = "sync.themes";
const char kSyncTypedUrls[] = "sync.typed_urls";
const char kSyncExtensions[] = "sync.extensions";
const char kSyncExtensionSettings[] = "sync.extension_settings";
const char kSyncSearchEngines[] = "sync.search_engines";
const char kSyncSessions[] = "sync.sessions";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const char kSyncManaged[] = "sync.managed";

// Boolean to prevent sync from automatically starting up.  This is
// used when sync is disabled by the user via the privacy dashboard.
const char kSyncSuppressStart[] = "sync.suppress_start";

// List of the currently acknowledged set of sync types, used to figure out
// if a new sync type has rolled out so we can notify the user.
const char kSyncAcknowledgedSyncTypes[] = "sync.acknowledged_types";

// Dictionary from sync model type (as an int) to max invalidation
// version (int64 represented as a string).
const char kSyncMaxInvalidationVersions[] = "sync.max_invalidation_versions";

// The GUID session sync will use to identify this client, even across sync
// disable/enable events.
const char kSyncSessionsGUID[] = "sync.session_sync_guid";

// Opaque state from the invalidation subsystem that is persisted via prefs.
// The value is base 64 encoded.
const char kInvalidatorInvalidationState[] = "invalidator.invalidation_state";

// List of {source, name, max invalidation version} tuples. source is an int,
// while max invalidation version is an int64; both are stored as string
// representations though.
const char kInvalidatorMaxInvalidationVersions[] =
    "invalidator.max_invalidation_versions";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
const char kSyncEncryptionBootstrapToken[] =
    "sync.encryption_bootstrap_token";

// Same as kSyncEncryptionBootstrapToken, but derived from the keystore key,
// so we don't have to do a GetKey command at restart.
const char kSyncKeystoreEncryptionBootstrapToken[] =
    "sync.keystore_encryption_bootstrap_token";

// Boolean tracking whether the user chose to specify a secondary encryption
// passphrase.
const char kSyncUsingSecondaryPassphrase[] = "sync.using_secondary_passphrase";

// String that identifies the user logged into sync and other google services.
const char kGoogleServicesUsername[] = "google.services.username";

// Local state pref containing a string regex that restricts which accounts
// can be used to log in to chrome (e.g. "*@google.com"). If missing or blank,
// all accounts are allowed (no restrictions).
const char kGoogleServicesUsernamePattern[] =
    "google.services.username_pattern";

#if !defined(OS_ANDROID)
// Tracks the number of times that we have shown the sync promo at startup.
const char kSyncPromoStartupCount[] = "sync_promo.startup_count";

// A counter to remember the number of times we've been to the sync promo page
// (not at startup).
const char kSyncPromoViewCount[] = "sync_promo.view_count";

// Boolean tracking whether the user chose to skip the sync promo.
const char kSyncPromoUserSkipped[] = "sync_promo.user_skipped";

// Boolean that specifies if the sync promo is allowed to show on first run.
// This preference is specified in the master preference file to suppress the
// sync promo for some installations.
const char kSyncPromoShowOnFirstRunAllowed[] =
    "sync_promo.show_on_first_run_allowed";

// Boolean that specifies if we should show a bubble in the new tab page.
// The bubble is used to confirm that the user is signed into sync.
const char kSyncPromoShowNTPBubble[] = "sync_promo.show_ntp_bubble";
#endif

// Time when the user's GAIA info was last updated (represented as an int64).
const char kProfileGAIAInfoUpdateTime[] = "profile.gaia_info_update_time";

// The URL from which the GAIA profile picture was downloaded. This is cached to
// prevent the same picture from being downloaded multiple times.
const char kProfileGAIAInfoPictureURL[] = "profile.gaia_info_picture_url";

// Create web application shortcut dialog preferences.
const char kWebAppCreateOnDesktop[] = "browser.web_app.create_on_desktop";
const char kWebAppCreateInAppsMenu[] = "browser.web_app.create_in_apps_menu";
const char kWebAppCreateInQuickLaunchBar[] =
    "browser.web_app.create_in_quick_launch_bar";

// Dictionary that maps Geolocation network provider server URLs to
// corresponding access token.
const char kGeolocationAccessToken[] = "geolocation.access_token";

// Boolean that indicates whether to allow firewall traversal while trying to
// establish the initial connection from the client or host.
const char kRemoteAccessHostFirewallTraversal[] =
    "remote_access.host_firewall_traversal";

// Boolean controlling whether 2-factor auth should be required when connecting
// to a host (instead of a PIN).
const char kRemoteAccessHostRequireTwoFactor[] =
    "remote_access.host_require_two_factor";

// String containing the domain name that hosts must belong to. If blank, then
// hosts can belong to any domain.
const char kRemoteAccessHostDomain[] = "remote_access.host_domain";

// String containing the domain name of the Chromoting Directory.
// Used by Chromoting host and client.
const char kRemoteAccessHostTalkGadgetPrefix[] =
    "remote_access.host_talkgadget_prefix";

// Boolean controlling whether curtaining is required when connecting to a host.
const char kRemoteAccessHostRequireCurtain[] =
    "remote_access.host_require_curtain";

// The last used printer and its settings.
const char kPrintPreviewStickySettings[] =
    "printing.print_preview_sticky_settings";
// The root URL of the cloud print service.
const char kCloudPrintServiceURL[] = "cloud_print.service_url";

// The URL to use to sign in to cloud print.
const char kCloudPrintSigninURL[] = "cloud_print.signin_url";

// The last requested size of the dialog as it was closed.
const char kCloudPrintDialogWidth[] = "cloud_print.dialog_size.width";
const char kCloudPrintDialogHeight[] = "cloud_print.dialog_size.height";
const char kCloudPrintSigninDialogWidth[] =
    "cloud_print.signin_dialog_size.width";
const char kCloudPrintSigninDialogHeight[] =
    "cloud_print.signin_dialog_size.height";

#if !defined(OS_ANDROID)
// The Chrome To Mobile service mobile device list pref.
const char kChromeToMobileDeviceList[] = "chrome_to_mobile.device_list";
#endif

// The list of BackgroundContents that should be loaded when the browser
// launches.
const char kRegisteredBackgroundContents[] = "background_contents.registered";

#if !defined(OS_ANDROID)
// An int that stores how often we've shown the "Chrome is configured to
// auto-launch" infobar.
const char kShownAutoLaunchInfobar[] = "browser.shown_autolaunch_infobar";
#endif

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

// Boolean that specifies whether to allow basic auth prompting on cross-
// domain sub-content requests.
const char kAllowCrossOriginAuthPrompt[] = "auth.allow_cross_origin_prompt";

#if defined(OS_CHROMEOS)
// Dictionary for transient storage of settings that should go into device
// settings storage before owner has been assigned.
const char kDeviceSettingsCache[] = "signed_settings_cache";

// The hardware keyboard layout of the device. This should look like
// "xkb:us::eng".
const char kHardwareKeyboardLayout[] = "intl.hardware_keyboard";

// An integer pref which shows number of times carrier deal promo
// notification has been shown to user.
const char kCarrierDealPromoShown[] =
    "settings.internet.mobile.carrier_deal_promo_shown";

// A boolean pref of the auto-enrollment decision. Its value is only valid if
// it's not the default value; otherwise, no auto-enrollment decision has been
// made yet.
const char kShouldAutoEnroll[] = "ShouldAutoEnroll";

// An integer pref with the maximum number of bits used by the client in a
// previous auto-enrollment request. If the client goes through an auto update
// during OOBE and reboots into a version of the OS with a larger maximum
// modulus, then it will retry auto-enrollment using the updated value.
const char kAutoEnrollmentPowerLimit[] = "AutoEnrollmentPowerLimit";

// The local state pref that stores device activity times before reporting
// them to the policy server.
const char kDeviceActivityTimes[] = "device_status.activity_times";

// A pref holding the last known location when device location reporting is
// enabled.
const char kDeviceLocation[] = "device_status.location";

// A string that is used to store first-time sync startup after once sync is
// disabled. This will be refreshed every sign-in.
const char kSyncSpareBootstrapToken[] = "sync.spare_bootstrap_token";

// A pref holding the value of the policy used to disable mounting of external
// storage for the user.
const char kExternalStorageDisabled[] = "hardware.external_storage_disabled";

// A pref holding the value of the policy used to disable playing audio on
// ChromeOS devices. This pref overrides |kAudioMute| but does not overwrite
// it, therefore when the policy is lifted the original mute state is restored.
const char kAudioOutputAllowed[] = "hardware.audio_output_enabled";

// A pref holding the value of the policy used to disable capturing audio on
// ChromeOS devices.
const char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";

// A dictionary that maps usernames to wallpaper properties.
const char kUsersWallpaperInfo[] = "user_wallpaper_info";
#endif

// Whether there is a Flash version installed that supports clearing LSO data.
const char kClearPluginLSODataEnabled[] = "browser.clear_lso_data_enabled";

// Whether we should show Pepper Flash-specific settings.
const char kPepperFlashSettingsEnabled[] =
    "browser.pepper_flash_settings_enabled";

// String which specifies where to store the disk cache.
const char kDiskCacheDir[] = "browser.disk_cache_dir";
// Pref name for the policy specifying the maximal cache size.
const char kDiskCacheSize[] = "browser.disk_cache_size";
// Pref name for the policy specifying the maximal media cache size.
const char kMediaCacheSize[] = "browser.media_cache_size";

// Specifies the release channel that the device should be locked to.
// Possible values: "stable-channel", "beta-channel", "dev-channel", or an
// empty string, in which case the value will be ignored.
// TODO(dubroy): This preference may not be necessary once
// http://crosbug.com/17015 is implemented and the update engine can just
// fetch the correct value from the policy.
const char kChromeOsReleaseChannel[] = "cros.system.releaseChannel";

// Value of the enums in TabStrip::LayoutType as an int.
const char kTabStripLayoutType[] = "tab_strip_layout_type";

// If true, cloud policy for the user is loaded once the user signs in.
const char kLoadCloudPolicyOnSignin[] = "policy.load_cloud_policy_on_signin";

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
// A boolean indicating whether we should poll for print jobs when don't have
// an XMPP connection (false by default).
const char kCloudPrintEnableJobPoll[] = "cloud_print.enable_job_poll";
const char kCloudPrintRobotRefreshToken[] = "cloud_print.robot_refresh_token";
const char kCloudPrintRobotEmail[] = "cloud_print.robot_email";
// Indicates whether the Mac Virtual driver is enabled.
const char kVirtualPrinterDriverEnabled[] = "cloud_print.enable_virtual_driver";
// A boolean indicating whether submitting jobs to Google Cloud Print is
// blocked by policy.
const char kCloudPrintSubmitEnabled[] = "cloud_print.submit_enabled";

// Preference to store proxy settings.
const char kProxy[] = "proxy";
const char kMaxConnectionsPerProxy[] = "net.max_connections_per_proxy";

// Preferences that are exclusively used to store managed values for default
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
const char kManagedDefaultGeolocationSetting[] =
    "profile.managed_default_content_settings.geolocation";
const char kManagedDefaultNotificationsSetting[] =
    "profile.managed_default_content_settings.notifications";
const char kManagedDefaultMediaStreamSetting[] =
    "profile.managed_default_content_settings.media_stream";

// Preferences that are exclusively used to store managed
// content settings patterns.
const char kManagedCookiesAllowedForUrls[] =
    "profile.managed_cookies_allowed_for_urls";
const char kManagedCookiesBlockedForUrls[] =
    "profile.managed_cookies_blocked_for_urls";
const char kManagedCookiesSessionOnlyForUrls[] =
    "profile.managed_cookies_sessiononly_for_urls";
const char kManagedImagesAllowedForUrls[] =
    "profile.managed_images_allowed_for_urls";
const char kManagedImagesBlockedForUrls[] =
    "profile.managed_images_blocked_for_urls";
const char kManagedJavaScriptAllowedForUrls[] =
    "profile.managed_javascript_allowed_for_urls";
const char kManagedJavaScriptBlockedForUrls[] =
    "profile.managed_javascript_blocked_for_urls";
const char kManagedPluginsAllowedForUrls[] =
    "profile.managed_plugins_allowed_for_urls";
const char kManagedPluginsBlockedForUrls[] =
    "profile.managed_plugins_blocked_for_urls";
const char kManagedPopupsAllowedForUrls[] =
    "profile.managed_popups_allowed_for_urls";
const char kManagedPopupsBlockedForUrls[] =
    "profile.managed_popups_blocked_for_urls";
const char kManagedNotificationsAllowedForUrls[] =
    "profile.managed_notifications_allowed_for_urls";
const char kManagedNotificationsBlockedForUrls[] =
    "profile.managed_notifications_blocked_for_urls";
const char kManagedAutoSelectCertificateForUrls[] =
    "profile.managed_auto_select_certificate_for_urls";

// Set to true if the user created a login item so we should not modify it when
// uninstalling background apps.
const char kUserCreatedLoginItem[] = "background_mode.user_created_login_item";

// Set to true if the user removed our login item so we should not create a new
// one when uninstalling background apps.
const char kUserRemovedLoginItem[] = "background_mode.user_removed_login_item";

// Set to true if background mode is enabled on this browser.
const char kBackgroundModeEnabled[] = "background_mode.enabled";

// List of protocol handlers.
const char kRegisteredProtocolHandlers[] =
  "custom_handlers.registered_protocol_handlers";

// List of protocol handlers the user has requested not to be asked about again.
const char kIgnoredProtocolHandlers[] =
  "custom_handlers.ignored_protocol_handlers";

// Whether user-specified handlers for protocols and content types can be
// specified.
const char kCustomHandlersEnabled[] = "custom_handlers.enabled";

// Integers that specify the policy refresh rate for device- and user-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
const char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";
const char kUserPolicyRefreshRate[] = "policy.user_refresh_rate";

// String that represents the recovery component last downloaded version. This
// takes the usual 'a.b.c.d' notation.
const char kRecoveryComponentVersion[] = "recovery_component.version";

// String that stores the component updater last known state. This is used for
// troubleshooting.
const char kComponentUpdaterState[] = "component_updater.state";

// Boolean that is true if Web Intents is enabled.
const char kWebIntentsEnabled[] = "webintents.enabled";

// The next media gallery ID to assign.
const char kMediaGalleriesUniqueId[] = "media_galleries.gallery_id";

// A list of dictionaries, where each dictionary represents a known media
// gallery.
const char kMediaGalleriesRememberedGalleries[] =
    "media_galleries.remembered_galleries";

#if defined(USE_AURA)
// String value corresponding to ash::Shell::ShelfAlignment.
const char kShelfAlignment[] = "shelf_alignment";
// String value corresponding to ash::Shell::ShelfAutoHideBehavior.
const char kShelfAutoHideBehavior[] =
    "auto_hide_behavior";
// Boolean value indicating whether to use default pinned apps.
const char kUseDefaultPinnedApps[] = "use_default_pinned_apps";
const char kPinnedLauncherApps[] =
    "pinned_launcher_apps";

const char kLongPressTimeInSeconds[] =
    "gesture.long_press_time_in_seconds";
const char kMaxDistanceBetweenTapsForDoubleTap[] =
    "gesture.max_distance_between_taps_for_double_tap";
const char kMaxDistanceForTwoFingerTapInPixels[] =
    "gesture.max_distance_for_two_finger_tap_in_pixels";
const char kMaxSecondsBetweenDoubleClick[] =
    "gesture.max_seconds_between_double_click";
const char kMaxSeparationForGestureTouchesInPixels[] =
    "gesture.max_separation_for_gesture_touches_in_pixels";
const char kMaxSwipeDeviationRatio[] =
    "gesture.max_swipe_deviation_ratio";
const char kMaxTouchDownDurationInSecondsForClick[] =
    "gesture.max_touch_down_duration_in_seconds_for_click";
const char kMaxTouchMoveInPixelsForClick[] =
    "gesture.max_touch_move_in_pixels_for_click";
const char kMinDistanceForPinchScrollInPixels[] =
    "gesture.min_distance_for_pinch_scroll_in_pixels";
const char kMinFlickSpeedSquared[] =
    "gesture.min_flick_speed_squared";
const char kMinPinchUpdateDistanceInPixels[] =
    "gesture.min_pinch_update_distance_in_pixels";
const char kMinRailBreakVelocity[] =
    "gesture.min_rail_break_velocity";
const char kMinScrollDeltaSquared[] =
    "gesture.min_scroll_delta_squared";
const char kMinSwipeSpeed[] =
    "gesture.min_swipe_speed";
const char kMinTouchDownDurationInSecondsForClick[] =
    "gesture.min_touch_down_duration_in_seconds_for_click";
const char kPointsBufferedForVelocity[] =
    "gesture.points_buffered_for_velocity";
const char kRailBreakProportion[] =
    "gesture.rail_break_proportion";
const char kRailStartProportion[] =
    "gesture.rail_start_proportion";
const char kSemiLongPressTimeInSeconds[] =
    "gesture.semi_long_press_time_in_seconds";
const char kTouchScreenFlingAccelerationAdjustment[] =
    "gesture.touchscreen_fling_acceleration_adjustment";
#endif

// Indicates whether the browser is in managed mode.
const char kInManagedMode[] = "managed_mode";

// Counts how many more times the 'profile on a network share' warning should be
// shown to the user before the next silence period.
const char kNetworkProfileWarningsLeft[] = "network_profile.warnings_left";
// Tracks the time of the last shown warning. Used to reset
// |network_profile.warnings_left| after a silence period.
const char kNetworkProfileLastWarningTime[] =
    "network_profile.last_warning_time";

}  // namespace prefs
