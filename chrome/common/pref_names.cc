// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"

#include "base/basictypes.h"
#include "chrome/common/pref_font_webkit_names.h"

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

// Disables screenshot accelerators and extension APIs.
// This setting resides both in profile prefs and local state. Accelerator
// handling code reads local state, while extension APIs use profile pref.
const char kDisableScreenshots[] = "disable_screenshots";

// If set to true profiles are created in ephemeral mode and do not store their
// data in the profile folder on disk but only in memory.
const char kForceEphemeralProfiles[] = "profile.ephemeral_mode";

// A boolean specifying whether the New Tab page is the home page or not.
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";

// This is the URL of the page to load when opening new tabs.
const char kHomePage[] = "homepage";

// An integer that keeps track of the profile icon version. This allows us to
// determine the state of the profile icon for icon format changes.
const char kProfileIconVersion[] = "profile.icon_version";

// Used to determine if the last session exited cleanly. Set to false when
// first opened, and to true when closing. On startup if the value is false,
// it means the profile didn't exit cleanly.
// DEPRECATED: this is replaced by kSessionExitType and exists for backwards
// compatibility.
const char kSessionExitedCleanly[] = "profile.exited_cleanly";

// A string pref whose values is one of the values defined by
// |ProfileImpl::kPrefExitTypeXXX|. Set to |kPrefExitTypeCrashed| on startup and
// one of |kPrefExitTypeNormal| or |kPrefExitTypeSessionEnded| during
// shutdown. Used to determine the exit type the last time the profile was open.
const char kSessionExitType[] = "profile.exit_type";

// An integer pref. Holds one of several values:
// 0: (deprecated) open the homepage on startup.
// 1: restore the last session.
// 2: this was used to indicate a specific session should be restored. It is
//    no longer used, but saved to avoid conflict with old preferences.
// 3: unused, previously indicated the user wants to restore a saved session.
// 4: restore the URLs defined in kURLsToRestoreOnStartup.
// 5: open the New Tab Page on startup.
const char kRestoreOnStartup[] = "session.restore_on_startup";

// A preference to keep track of whether we have already checked whether we
// need to migrate the user from kRestoreOnStartup=0 to kRestoreOnStartup=4.
// We only need to do this check once, on upgrade from m18 or lower to m19 or
// higher.
const char kRestoreOnStartupMigrated[] = "session.restore_on_startup_migrated";

// Serialized migration time of kURLsToRestoreOnStartup (see
// base::Time::ToInternalValue for details on serialization format).
const char kRestoreStartupURLsMigrationTime[] =
    "session.startup_urls_migration_time";

// The URLs to restore on startup or when the home button is pressed. The URLs
// are only restored on startup if kRestoreOnStartup is 4.
const char kURLsToRestoreOnStartup[] = "session.startup_urls";

// Old startup url pref name for kURLsToRestoreOnStartup.
const char kURLsToRestoreOnStartupOld[] = "session.urls_to_restore_on_startup";

// Stores the email address associated with the google account of the custodian
// of the supervised user, set when the supervised user is created.
const char kSupervisedUserCustodianEmail[] = "profile.managed.custodian_email";

// Stores the display name associated with the google account of the custodian
// of the supervised user, updated (if possible) each time the supervised user
// starts a session.
const char kSupervisedUserCustodianName[] = "profile.managed.custodian_name";

// Stores the URL of the profile image associated with the google account of the
// custodian of the supervised user.
const char kSupervisedUserCustodianProfileImageURL[] =
    "profile.managed.custodian_profile_image_url";

// Maps host names to whether the host is manually allowed or blocked.
const char kSupervisedUserManualHosts[] = "profile.managed.manual_hosts";

// Maps URLs to whether the URL is manually allowed or blocked.
const char kSupervisedUserManualURLs[] = "profile.managed.manual_urls";

// Stores the email address associated with the google account of the secondary
// custodian of the supervised user, set when the supervised user is created.
const char kSupervisedUserSecondCustodianEmail[] =
    "profile.managed.second_custodian_email";

// Stores the display name associated with the google account of the secondary
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
const char kSupervisedUserSecondCustodianName[] =
    "profile.managed.second_custodian_name";

// Stores the URL of the profile image associated with the google account of the
// secondary custodian of the supervised user.
const char kSupervisedUserSecondCustodianProfileImageURL[] =
    "profile.managed.second_custodian_profile_image_url";

// Stores settings that can be modified both by a supervised user and their
// manager. See SupervisedUserSharedSettingsService for a description of
// the format.
const char kSupervisedUserSharedSettings[] = "profile.managed.shared_settings";

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

// If these change, the corresponding enums in the extension API
// experimental.fontSettings.json must also change.
const char* const kWebKitScriptsForFontFamilyMaps[] = {
#define EXPAND_SCRIPT_FONT(x, script_name) script_name ,
#include "chrome/common/pref_font_script_names-inl.h"
ALL_FONT_SCRIPTS("unused param")
#undef EXPAND_SCRIPT_FONT
};

const size_t kWebKitScriptsForFontFamilyMapsLength =
    arraysize(kWebKitScriptsForFontFamilyMaps);

// Strings for WebKit font family preferences. If these change, the pref prefix
// in pref_names_util.cc and the pref format in font_settings_api.cc must also
// change.
const char kWebKitStandardFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_STANDARD;
const char kWebKitFixedFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_FIXED;
const char kWebKitSerifFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_SERIF;
const char kWebKitSansSerifFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_SANSERIF;
const char kWebKitCursiveFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_CURSIVE;
const char kWebKitFantasyFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_FANTASY;
const char kWebKitPictographFontFamilyMap[] =
    WEBKIT_WEBPREFS_FONTS_PICTOGRAPH;
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

// WebKit preferences.
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
#if defined(OS_ANDROID)
const char kWebKitFontScaleFactor[] = "webkit.webprefs.font_scale_factor";
const char kWebKitForceEnableZoom[] = "webkit.webprefs.force_enable_zoom";
const char kWebKitPasswordEchoEnabled[] =
    "webkit.webprefs.password_echo_enabled";
#endif

const char kWebKitCommonScript[] = "Zyyy";
const char kWebKitStandardFontFamily[] = "webkit.webprefs.fonts.standard.Zyyy";
const char kWebKitFixedFontFamily[] = "webkit.webprefs.fonts.fixed.Zyyy";
const char kWebKitSerifFontFamily[] = "webkit.webprefs.fonts.serif.Zyyy";
const char kWebKitSansSerifFontFamily[] =
    "webkit.webprefs.fonts.sansserif.Zyyy";
const char kWebKitCursiveFontFamily[] = "webkit.webprefs.fonts.cursive.Zyyy";
const char kWebKitFantasyFontFamily[] = "webkit.webprefs.fonts.fantasy.Zyyy";
const char kWebKitPictographFontFamily[] =
    "webkit.webprefs.fonts.pictograph.Zyyy";
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

// Boolean that is true when SafeBrowsing is enabled.
const char kSafeBrowsingEnabled[] = "safebrowsing.enabled";

// Boolean that tell us whether malicious download feedback is enabled.
const char kSafeBrowsingExtendedReportingEnabled[] =
    "safebrowsing.extended_reporting_enabled";

// Boolean that tell us whether malicious download feedback is enabled.
// TODO(felt): Deprecate. crbug.com/383866
const char kSafeBrowsingDownloadFeedbackEnabled[] =
    "safebrowsing.download_feedback_enabled";

// Boolean that is true when SafeBrowsing Malware Report is enabled.
// TODO(felt): Deprecate. crbug.com/383866
const char kSafeBrowsingReportingEnabled[] =
    "safebrowsing.reporting_enabled";

// Boolean that is true when the SafeBrowsing interstitial should not allow
// users to proceed anyway.
const char kSafeBrowsingProceedAnywayDisabled[] =
    "safebrowsing.proceed_anyway_disabled";

// Boolean that is true when SafeBrowsing has sent an incident report.
const char kSafeBrowsingIncidentReportSent[] =
    "safebrowsing.incident_report_sent";

// A dictionary mapping incident types to a dict of incident key:digest pairs.
const char kSafeBrowsingIncidentsSent[] = "safebrowsing.incidents_sent";

// Enum that specifies whether Incognito mode is:
// 0 - Enabled. Default behaviour. Default mode is available on demand.
// 1 - Disabled. Used cannot browse pages in Incognito mode.
// 2 - Forced. All pages/sessions are forced into Incognito.
const char kIncognitoModeAvailability[] = "incognito.mode_availability";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

#if defined(OS_ANDROID)
// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";
#endif

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

#if defined(OS_ANDROID) || defined(OS_IOS)
// Last time that a check for cloud policy management was done. This time is
// recorded on Android so that retries aren't attempted on every startup.
// Instead the cloud policy registration is retried at least 1 or 3 days later.
const char kLastPolicyCheckTime[] = "policy.last_policy_check_time";
#endif

// Prefix URL for the experimental Instant ZeroSuggest provider.
const char kInstantUIZeroSuggestUrlPrefix[] =
    "instant_ui.zero_suggest_url_prefix";

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
// Actions include DNS prefetching, TCP and SSL preconnection, prerendering
// of web pages, and resource prefetching.
// NOTE: The "dns_prefetching.enabled" value is used so that historical user
// preferences are not lost.
// TODO(bnc): Remove kNetworkPredictionEnabled once kNetworkPredictionOptions
// is functioning as per crbug.com/334602.
const char kNetworkPredictionEnabled[] = "dns_prefetching.enabled";

// A preference of enum chrome_browser_net::NetworkPredictionOptions shows
// if prediction of network actions is allowed, depending on network type.
// Actions include DNS prefetching, TCP and SSL preconnection, prerendering
// of web pages, and resource prefetching.
// TODO(bnc): Implement this preference as per crbug.com/334602.
const char kNetworkPredictionOptions[] = "net.network_prediction_options";

// An integer representing the state of the default apps installation process.
// This value is persisted in the profile's user preferences because the process
// is async, and the user may have stopped chrome in the middle.  The next time
// the profile is opened, the process will continue from where it left off.
//
// See possible values in external_provider_impl.cc.
const char kDefaultAppsInstallState[] = "default_apps_install_state";

// A boolean pref set to true if the Chrome Web Store icons should be hidden
// from the New Tab Page and app launcher.
const char kHideWebStoreIcon[] = "hide_web_store_icon";

#if defined(OS_CHROMEOS)
// A dictionary pref to hold the mute setting for all the currently known
// audio devices.
const char kAudioDevicesMute[] = "settings.audio.devices.mute";

// A dictionary pref storing the volume settings for all the currently known
// audio devices.
const char kAudioDevicesVolumePercent[] =
    "settings.audio.devices.volume_percent";

// An integer pref to initially mute volume if 1. This pref is ignored if
// |kAudioOutputAllowed| is set to false, but its value is preserved, therefore
// when the policy is lifted the original mute state is restored.  This setting
// is here only for migration purposes now. It is being replaced by the
// |kAudioDevicesMute| setting.
const char kAudioMute[] = "settings.audio.mute";

// A double pref storing the user-requested volume. This setting is here only
// for migration purposes now. It is being replaced by the
// |kAudioDevicesVolumePercent| setting.
const char kAudioVolumePercent[] = "settings.audio.volume_percent";

// An integer pref to record user's spring charger check result.
// 0 - unknown charger, not checked yet.
// 1 - confirmed safe charger.
// 2 - confirmed original charger and declined to order new charger.
// 3 - confirmed original charger and ordered new charger online.
// 4 - confirmed original charger and ordered new charger by phone.
// 5 - confirmed original charger, ordered a new one online, but continue to use
//     the old one.
// 6 - confirmed original charger, ordered a new one by phone, but continue to
//     use the old one.
const char kSpringChargerCheck[] = "settings.spring_charger.check_result";

// A boolean pref set to true if touchpad tap-to-click is enabled.
const char kTapToClickEnabled[] = "settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if touchpad tap-dragging is enabled.
const char kTapDraggingEnabled[] = "settings.touchpad.enable_tap_dragging";

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

// A boolean pref to disable Google Drive integration.
// The pref prefix should remain as "gdata" for backward compatibility.
const char kDisableDrive[] = "gdata.disabled";

// A boolean pref to disable Drive over cellular connections.
// The pref prefix should remain as "gdata" for backward compatibility.
const char kDisableDriveOverCellular[] = "gdata.cellular.disabled";

// A boolean pref to disable hosted files on Drive.
// The pref prefix should remain as "gdata" for backward compatibility.
const char kDisableDriveHostedFiles[] = "gdata.hosted_files.disabled";

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

// A List pref (comma-separated list) set to the extension IMEs to be enabled.
const char kLanguageEnabledExtensionImes[] =
    "settings.language.enabled_extension_imes";

// Integer prefs which determine how we remap modifier keys (e.g. swap Alt and
// Control.) Possible values for these prefs are 0-4. See ModifierKey enum in
// src/chrome/browser/chromeos/input_method/xkeyboard.h
const char kLanguageRemapSearchKeyTo[] =
    // Note: we no longer use XKB for remapping these keys, but we can't change
    // the pref names since the names are already synced with the cloud.
    "settings.language.xkb_remap_search_key_to";
const char kLanguageRemapControlKeyTo[] =
    "settings.language.xkb_remap_control_key_to";
const char kLanguageRemapAltKeyTo[] =
    "settings.language.xkb_remap_alt_key_to";
const char kLanguageRemapCapsLockKeyTo[] =
    "settings.language.remap_caps_lock_key_to";
const char kLanguageRemapDiamondKeyTo[] =
    "settings.language.remap_diamond_key_to";

// A boolean pref that causes top-row keys to be interpreted as function keys
// instead of as media keys.
const char kLanguageSendFunctionKeys[] =
    "settings.language.send_function_keys";

// A boolean pref which determines whether key repeat is enabled.
const char kLanguageXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";
// A integer pref which determines key repeat delay (in ms).
const char kLanguageXkbAutoRepeatDelay[] =
    "settings.language.xkb_auto_repeat_delay_r2";
// A integer pref which determines key repeat interval (in ms).
const char kLanguageXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes were added to the three prefs above when we changed the
// preferences to not be user-configurable or sync with the cloud. The prefs are
// now user-configurable and syncable again, but we don't want to overwrite the
// current values with the old synced values, so we continue to use this suffix.

// A boolean pref which determines whether the large cursor feature is enabled.
const char kAccessibilityLargeCursorEnabled[] =
    "settings.a11y.large_cursor_enabled";

// A boolean pref which determines whether the sticky keys feature is enabled.
const char kAccessibilityStickyKeysEnabled[] =
    "settings.a11y.sticky_keys_enabled";
// A boolean pref which determines whether spoken feedback is enabled.
const char kAccessibilitySpokenFeedbackEnabled[] = "settings.accessibility";
// A boolean pref which determines whether high conrast is enabled.
const char kAccessibilityHighContrastEnabled[] =
    "settings.a11y.high_contrast_enabled";
// A boolean pref which determines whether screen magnifier is enabled.
const char kAccessibilityScreenMagnifierEnabled[] =
    "settings.a11y.screen_magnifier";
// A integer pref which determines what type of screen magnifier is enabled.
// Note that: 'screen_magnifier_type' had been used as string pref. Hence,
// we are using another name pref here.
const char kAccessibilityScreenMagnifierType[] =
    "settings.a11y.screen_magnifier_type2";
// A double pref which determines a zooming scale of the screen magnifier.
const char kAccessibilityScreenMagnifierScale[] =
    "settings.a11y.screen_magnifier_scale";
// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
const char kAccessibilityVirtualKeyboardEnabled[] =
    "settings.a11y.virtual_keyboard";
// A boolean pref which determines whether autoclick is enabled.
const char kAccessibilityAutoclickEnabled[] = "settings.a11y.autoclick";
// An integer pref which determines time in ms between when the mouse cursor
// stops and when an autoclick is triggered.
const char kAccessibilityAutoclickDelayMs[] =
    "settings.a11y.autoclick_delay_ms";
// A boolean pref which determines whether the accessibility menu shows
// regardless of the state of a11y features.
const char kShouldAlwaysShowAccessibilityMenu[] = "settings.a11y.enable_menu";

// A boolean pref which turns on Advanced Filesystem
// (USB support, SD card, etc).
const char kLabsAdvancedFilesystemEnabled[] =
    "settings.labs.advanced_filesystem";

// A boolean pref which turns on the mediaplayer.
const char kLabsMediaplayerEnabled[] = "settings.labs.mediaplayer";

// A boolean pref that turns on automatic screen locking.
const char kEnableAutoScreenLock[] = "settings.enable_screen_lock";

// A boolean pref of whether to show mobile plan notifications.
const char kShowPlanNotifications[] =
    "settings.internet.mobile.show_plan_notifications";

// A boolean pref of whether to show 3G promo notification.
const char kShow3gPromoNotification[] =
    "settings.internet.mobile.show_3g_promo_notification";

// A string pref that contains version where "What's new" promo was shown.
const char kChromeOSReleaseNotesVersion[] = "settings.release_notes.version";

// A boolean pref that controls whether proxy settings from shared network
// settings (accordingly from device policy) are applied or ignored.
const char kUseSharedProxies[] = "settings.use_shared_proxies";

// Power state of the current displays from the last run.
const char kDisplayPowerState[] = "settings.display.power_state";
// A dictionary pref that stores per display preferences.
const char kDisplayProperties[] = "settings.display.properties";

// A dictionary pref that specifies per-display layout/offset information.
// Its key is the ID of the display and its value is a dictionary for the
// layout/offset information.
const char kSecondaryDisplays[] = "settings.display.secondary_displays";

// A boolean pref indicating whether user activity has been observed in the
// current session already. The pref is used to restore information about user
// activity after browser crashes.
const char kSessionUserActivitySeen[] = "session.user_activity_seen";

// A preference to keep track of the session start time. If the session length
// limit is configured to start running after initial user activity has been
// observed, the pref is set after the first user activity in a session.
// Otherwise, it is set immediately after session start. The pref is used to
// restore the session start time after browser crashes. The time is expressed
// as the serialization obtained from base::TimeTicks::ToInternalValue().
const char kSessionStartTime[] = "session.start_time";

// Holds the maximum session time in milliseconds. If this pref is set, the
// user is logged out when the maximum session time is reached. The user is
// informed about the remaining time by a countdown timer shown in the ash
// system tray.
const char kSessionLengthLimit[] = "session.length_limit";

// Whether the session length limit should start running only after the first
// user activity has been observed in a session.
const char kSessionWaitForInitialUserActivity[] =
    "session.wait_for_initial_user_activity";

// Inactivity time in milliseconds while the system is on AC power before
// the screen should be dimmed, turned off, or locked, before an
// IdleActionImminent D-Bus signal should be sent, or before
// kPowerAcIdleAction should be performed.  0 disables the delay (N/A for
// kPowerAcIdleDelayMs).
const char kPowerAcScreenDimDelayMs[] = "power.ac_screen_dim_delay_ms";
const char kPowerAcScreenOffDelayMs[] = "power.ac_screen_off_delay_ms";
const char kPowerAcScreenLockDelayMs[] = "power.ac_screen_lock_delay_ms";
const char kPowerAcIdleWarningDelayMs[] = "power.ac_idle_warning_delay_ms";
const char kPowerAcIdleDelayMs[] = "power.ac_idle_delay_ms";

// Similar delays while the system is on battery power.
const char kPowerBatteryScreenDimDelayMs[] =
    "power.battery_screen_dim_delay_ms";
const char kPowerBatteryScreenOffDelayMs[] =
    "power.battery_screen_off_delay_ms";
const char kPowerBatteryScreenLockDelayMs[] =
    "power.battery_screen_lock_delay_ms";
const char kPowerBatteryIdleWarningDelayMs[] =
    "power.battery_idle_warning_delay_ms";
const char kPowerBatteryIdleDelayMs[] =
    "power.battery_idle_delay_ms";

// Action that should be performed when the idle delay is reached while the
// system is on AC power or battery power.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerAcIdleAction[] = "power.ac_idle_action";
const char kPowerBatteryIdleAction[] = "power.battery_idle_action";

// Action that should be performed when the lid is closed.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerLidClosedAction[] = "power.lid_closed_action";

// Should audio and video activity be used to disable the above delays?
const char kPowerUseAudioActivity[] = "power.use_audio_activity";
const char kPowerUseVideoActivity[] = "power.use_video_activity";

// Should extensions be able to use the chrome.power API to override
// screen-related power management (including locking)?
const char kPowerAllowScreenWakeLocks[] = "power.allow_screen_wake_locks";

// Amount by which the screen-dim delay should be scaled while the system
// is in presentation mode. Values are limited to a minimum of 1.0.
const char kPowerPresentationScreenDimDelayFactor[] =
    "power.presentation_screen_dim_delay_factor";

// Amount by which the screen-dim delay should be scaled when user activity is
// observed while the screen is dimmed or soon after the screen has been turned
// off.  Values are limited to a minimum of 1.0.
const char kPowerUserActivityScreenDimDelayFactor[] =
    "power.user_activity_screen_dim_delay_factor";

// Whether the power management delays should start running only after the first
// user activity has been observed in a session.
const char kPowerWaitForInitialUserActivity[] =
    "power.wait_for_initial_user_activity";

// The URL from which the Terms of Service can be downloaded. The value is only
// honored for public accounts.
const char kTermsOfServiceURL[] = "terms_of_service.url";

// Indicates that the Profile has made navigations that used a certificate
// installed by the system administrator. If that is true then the local cache
// of remote data is tainted (e.g. shared scripts), and future navigations
// show a warning indicating that the organization may track the browsing
// session.
const char kUsedPolicyCertificatesOnce[] = "used_policy_certificates_once";

// Indicates whether the remote attestation is enabled for the user.
const char kAttestationEnabled[] = "attestation.enabled";
// The list of extensions allowed to use the platformKeysPrivate API for
// remote attestation.
const char kAttestationExtensionWhitelist[] = "attestation.extension_whitelist";

// A boolean pref indicating whether the projection touch HUD is enabled or not.
const char kTouchHudProjectionEnabled[] = "touch_hud.projection_enabled";

// A pref to configure networks. Its value must be a list of
// NetworkConfigurations according to the OpenNetworkConfiguration
// specification.
// Currently, this pref is only used to store the policy. The user's
// configuration is still stored in Shill.
const char kOpenNetworkConfiguration[] = "onc";

// A boolean pref that tracks whether the user has already given consent for
// enabling remote attestation for content protection.
const char kRAConsentFirstTime[] = "settings.privacy.ra_consent";

// A boolean pref recording whether user has dismissed the multiprofile
// itroduction dialog show.
const char kMultiProfileNeverShowIntro[] =
    "settings.multi_profile_never_show_intro";

// A boolean pref recording whether user has dismissed the multiprofile
// teleport warning dialog show.
const char kMultiProfileWarningShowDismissed[] =
    "settings.multi_profile_warning_show_dismissed";

// A string pref that holds string enum values of how the user should behave
// in a multiprofile session. See ChromeOsMultiProfileUserBehavior policy
// for more details of the valid values.
const char kMultiProfileUserBehavior[] = "settings.multiprofile_user_behavior";

// A boolean preference indicating whether user has seen first-run tutorial
// already.
const char kFirstRunTutorialShown[] = "settings.first_run_tutorial_shown";

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again. The time is expressed in
// seconds. A value of -1 indicates no limit, allowing the user to use offline
// authentication indefinitely. The limit is in effect only if GAIA redirected
// the user to a SAML IdP during the last online authentication.
const char kSAMLOfflineSigninTimeLimit[] = "saml.offline_signin_time_limit";

// A preference to keep track of the last time the user authenticated against
// GAIA using SAML. The preference is updated whenever the user authenticates
// against GAIA: If GAIA redirects to a SAML IdP, the preference is set to the
// current time. If GAIA performs the authentication itself, the preference is
// cleared. The time is expressed as the serialization obtained from
// base::Time::ToInternalValue().
const char kSAMLLastGAIASignInTime[] = "saml.last_gaia_sign_in_time";

// The total number of seconds that the machine has spent sitting on the
// OOBE screen.
const char kTimeOnOobe[] = "settings.time_on_oobe";

// The app/extension name who sets the current wallpaper. If current wallpaper
// is set by the component wallpaper picker, it is set to an empty string.
const char kCurrentWallpaperAppName[] = "wallpaper.app.name";

// List of mounted file systems via the File System Provider API. Used to
// restore them after a reboot.
const char kFileSystemProviderMounted[] = "file_system_provider.mounted";

// A boolean pref set to true if the virtual keyboard should be enabled.
const char kTouchVirtualKeyboardEnabled[] = "ui.touch_virtual_keyboard_enabled";

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
const char kLastClearBrowsingDataTime[] =
    "browser.last_clear_browsing_data_time";

// Boolean pref to define the default values for using spellchecker.
const char kEnableContinuousSpellcheck[] = "browser.enable_spellchecking";

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

// Boolean controlling whether deleting browsing and download history is
// permitted.
const char kAllowDeletingBrowserHistory[] = "history.deleting_enabled";

// Boolean controlling whether SafeSearch is mandatory for Google Web Searches.
const char kForceSafeSearch[] = "settings.force_safesearch";

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Linux specific preference on whether we should match the system theme.
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

// Boolean pref which indicates whether the Chrome Apps & Extensions Developer
// Tool promotion has been dismissed by the user.
const char kExtensionsUIDismissedADTPromo[] =
    "extensions.ui.dismissed_adt_promo";

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

// When bundled NPAPI Flash is removed, if at that point it is enabled while
// Pepper Flash is disabled, we would like to turn on Pepper Flash. And we will
// want to do so only once.
const char kPluginsMigratedToPepperFlash[] = "plugins.migrated_to_pepper_flash";

// In the early stage of component-updated PPAPI Flash, we did field trials in
// which it was set to disabled by default. The corresponding settings item may
// remain in some users' profiles. Currently it affects both the bundled and
// component-updated PPAPI Flash (since the two share the same enable/disable
// state). We want to remove this item to get those users to use PPAPI Flash.
// We will want to do so only once.
const char kPluginsRemovedOldComponentPepperFlashSettings[] =
    "plugins.removed_old_component_pepper_flash_settings";

#if !defined(OS_ANDROID)
// Whether about:plugins is shown in the details mode or not.
const char kPluginsShowDetails[] = "plugins.show_details";
#endif

// Boolean that indicates whether outdated plugins are allowed or not.
const char kPluginsAllowOutdated[] = "plugins.allow_outdated";

// Boolean that indicates whether plugins that require authorization should
// be always allowed or not.
const char kPluginsAlwaysAuthorize[] = "plugins.always_authorize";

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Dictionary holding plug-ins metadata.
const char kPluginsMetadata[] = "plugins.metadata";

// Last update time of plug-ins resource cache.
const char kPluginsResourceCacheUpdate[] = "plugins.resource_cache_update";
#endif

// Boolean that indicates whether we should check if we are the default browser
// on start-up.
const char kCheckDefaultBrowser[] = "browser.check_default_browser";

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

// Dictionary of content settings applied to all hosts by default.
const char kDefaultContentSettings[] = "profile.default_content_settings";

// Boolean indicating whether the clear on exit pref was migrated to content
// settings yet.
const char kContentSettingsClearOnExitMigrated[] =
    "profile.content_settings.clear_on_exit_migrated";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Patterns for mapping origins to origin related settings. Default settings
// will be applied to origins that don't match any of the patterns. The pattern
// format used is defined by kContentSettingsVersion.
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

// A dictionary that tracks the default data model to use for each section of
// the dialog.
const char kAutofillDialogAutofillDefault[] = "autofill.data_model_default";

// Whether a user opted out of making purchases with Google Wallet; changed via
// the autofill dialog's account chooser and set explicitly on dialog submission
// (but not cancel). If this isn't set, the dialog assumes it's the first run.
const char kAutofillDialogPayWithoutWallet[] = "autofill.pay_without_wallet";

// Which GAIA users have accepted that use of Google Wallet implies their
// location will be shared with fraud protection services.
const char kAutofillDialogWalletLocationAcceptance[] =
    "autofill.wallet_location_disclosure";

// Whether a user wants to save data locally in Autofill.
const char kAutofillDialogSaveData[] = "autofill.save_data";

// Whether the user has selected "Same as billing" for the shipping address when
// using Google Wallet.
const char kAutofillDialogWalletShippingSameAsBilling[] =
    "autofill.wallet_shipping_same_as_billing";

// The number of times the generated credit card bubble has been shown.
const char kAutofillGeneratedCardBubbleTimesShown[] =
    "autofill.generated_card_bubble_times_shown";

// A dictionary that tracks the defaults to be set on the next invocation
// of the requestAutocomplete dialog.
const char kAutofillDialogDefaults[] = "autofill.rac_dialog_defaults";

#if !defined(OS_ANDROID)
const char kPinnedTabs[] = "pinned_tabs";
#endif

#if defined(OS_ANDROID)
// Boolean that controls the enabled-state of Geolocation in content.
const char kGeolocationEnabled[] = "geolocation.enabled";
#endif

#if defined(ENABLE_GOOGLE_NOW)
// Boolean that is true when Google services can use the user's location.
const char kGoogleGeolocationAccessEnabled[] =
    "googlegeolocationaccess.enabled";
#endif

// The default audio capture device used by the Media content setting.
const char kDefaultAudioCaptureDevice[] = "media.default_audio_capture_device";

// The default video capture device used by the Media content setting.
const char kDefaultVideoCaptureDevice[] = "media.default_video_capture_Device";

// The salt used for creating random MediaSource IDs.
const char kMediaDeviceIdSalt[] = "media.device_id_salt";

// Preference to disable 3D APIs (WebGL, Pepper 3D).
const char kDisable3DAPIs[] = "disable_3d_apis";

const char kEnableDeprecatedWebPlatformFeatures[] =
    "enable_deprecated_web_platform_features";

// Whether to enable hyperlink auditing ("<a ping>").
const char kEnableHyperlinkAuditing[] = "enable_a_ping";

// Whether to enable sending referrers.
const char kEnableReferrers[] = "enable_referrers";

// Whether to send the DNT header.
const char kEnableDoNotTrack[] = "enable_do_not_track";

// GL_VENDOR string.
const char kGLVendorString[] = "gl_vendor_string";

// GL_RENDERER string.
const char kGLRendererString[] = "gl_renderer_string";

// GL_VERSION string.
const char kGLVersionString[] = "gl_version_string";

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

// Profile avatar and name
const char kProfileAvatarIndex[] = "profile.avatar_index";
const char kProfileName[] = "profile.name";

// The supervised user ID.
const char kSupervisedUserId[] = "profile.managed_user_id";

// 64-bit integer serialization of the base::Time when the user's GAIA info
// was last updated.
const char kProfileGAIAInfoUpdateTime[] = "profile.gaia_info_update_time";

// The URL from which the GAIA profile picture was downloaded. This is cached to
// prevent the same picture from being downloaded multiple times.
const char kProfileGAIAInfoPictureURL[] = "profile.gaia_info_picture_url";

// Integer that specifies the number of times that we have shown the upgrade
// tutorial card in the avatar menu bubble.
const char kProfileAvatarTutorialShown[] =
    "profile.avatar_bubble_tutorial_shown";

// Indicates if we've already shown a notification that high contrast
// mode is on, recommending high-contrast extensions and themes.
const char kInvertNotificationShown[] = "invert_notification_version_2_shown";

// Boolean controlling whether printing is enabled.
const char kPrintingEnabled[] = "printing.enabled";

// Boolean controlling whether print preview is disabled.
const char kPrintPreviewDisabled[] = "printing.print_preview_disabled";

// An integer pref specifying the fallback behavior for sites outside of content
// packs. One of:
// 0: Allow (does nothing)
// 1: Warn.
// 2: Block.
const char kDefaultSupervisedUserFilteringBehavior[] =
    "profile.managed.default_filtering_behavior";

// Whether this user is permitted to create supervised users.
const char kSupervisedUserCreationAllowed[] =
    "profile.managed_user_creation_allowed";

// List pref containing the users supervised by this user.
const char kSupervisedUsers[] = "profile.managed_users";

// List pref containing the extension ids which are not allowed to send
// notifications to the message center.
const char kMessageCenterDisabledExtensionIds[] =
    "message_center.disabled_extension_ids";

// List pref containing the system component ids which are not allowed to send
// notifications to the message center.
const char kMessageCenterDisabledSystemComponentIds[] =
    "message_center.disabled_system_component_ids";

// Boolean pref indicating the Chrome Now welcome notification was dismissed
// by the user. Syncable.
// Note: This is now read-only. The welcome notification writes the _local
// version, below.
extern const char kWelcomeNotificationDismissed[] =
    "message_center.welcome_notification_dismissed";

// Boolean pref indicating the Chrome Now welcome notification was dismissed
// by the user on this machine.
extern const char kWelcomeNotificationDismissedLocal[] =
    "message_center.welcome_notification_dismissed_local";

// Boolean pref indicating the welcome notification was previously popped up.
extern const char kWelcomeNotificationPreviouslyPoppedUp[] =
    "message_center.welcome_notification_previously_popped_up";

// Integer pref containing the expiration timestamp of the welcome notification.
extern const char kWelcomeNotificationExpirationTimestamp[] =
    "message_center.welcome_notification_expiration_timestamp";

// Boolean pref that determines whether the user can enter fullscreen mode.
// Disabling fullscreen mode also makes kiosk mode unavailable on desktop
// platforms.
extern const char kFullscreenAllowed[] = "fullscreen.allowed";

// Enable notifications for new devices on the local network that can be
// registered to the user's account, e.g. Google Cloud Print printers.
const char kLocalDiscoveryNotificationsEnabled[] =
    "local_discovery.notifications_enabled";

// A timestamp (stored in base::Time::ToInternalValue format) of the last time
// a preference was reset.
const char kPreferenceResetTime[] = "prefs.preference_reset_time";

// The GCM channel's enabled state.
const char kGCMChannelEnabled[] = "gcm.channel_enabled";

// How many Service Workers are registered with the Push API (could be zero).
const char kPushMessagingRegistrationCount[] =
    "gcm.push_messaging_registration_count";

// Whether Easy Unlock is enabled.
const char kEasyUnlockEnabled[] = "easy_unlock.enabled";

// Whether to show the Easy Unlock first run tutorial.
const char kEasyUnlockShowTutorial[] = "easy_unlock.show_tutorial";

// Preference storing Easy Unlock pairing data.
const char kEasyUnlockPairing[] = "easy_unlock.pairing";

// Whether a user is allowed to use Easy Unlock.
const char kEasyUnlockAllowed[] = "easy_unlock.allowed";

// A cache of zero suggest results using JSON serialized into a string.
const char kZeroSuggestCachedResults[] = "zerosuggest.cachedresults";

// *************** LOCAL STATE ***************
// These are attached to the machine/installation

// A pref to configure networks device-wide. Its value must be a list of
// NetworkConfigurations according to the OpenNetworkConfiguration
// specification.
// Currently, this pref is only used to store the policy. The user's
// configuration is still stored in Shill.
const char kDeviceOpenNetworkConfiguration[] = "device_onc";

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
const char kCertRevocationCheckingRequiredLocalAnchors[] =
    "ssl.rev_checking.required_for_local_anchors";
const char kSSLVersionMin[] = "ssl.version_min";
const char kSSLVersionMax[] = "ssl.version_max";
const char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";
const char kDisableSSLRecordSplitting[] = "ssl.ssl_record_splitting.disabled";

// Dictionary of dates when a site's SSL blocking interstitial was proceeded
// through.
const char kSSLBlockingBypassed[] = "ssl.ssl_blocking_bypassed";

// A boolean pref of the EULA accepted flag.
const char kEulaAccepted[] = "EulaAccepted";

// Boolean that specifies whether or not crash reporting and metrics reporting
// are sent over the network for analysis.
const char kMetricsReportingEnabled[] =
    "user_experience_metrics.reporting_enabled";

// Boolean that specifies whether or not crash reports are sent
// over the network for analysis.
#if defined(OS_ANDROID)
const char kCrashReportingEnabled[] =
    "user_experience_metrics_crash.reporting_enabled";
#endif

// 64-bit integer serialization of the base::Time from the last successful seed
// fetch (i.e. when the Variations server responds with 200 or 304).
const char kVariationsLastFetchTime[] = "variations_last_fetch_time";

// String for the restrict parameter to be appended to the variations URL.
const char kVariationsRestrictParameter[] = "variations_restrict_parameter";

// String serialized form of variations seed protobuf.
const char kVariationsSeed[] = "variations_seed";

// 64-bit integer serialization of the base::Time from the last seed received.
const char kVariationsSeedDate[] = "variations_seed_date";

// SHA-1 hash of the serialized variations seed data (hex encoded).
const char kVariationsSeedHash[] = "variations_seed_hash";

// Digital signature of the binary variations seed data, base64-encoded.
const char kVariationsSeedSignature[] = "variations_seed_signature";

// Number of times a page load event occurred since the last report.
const char kStabilityPageLoadCount[] =
    "user_experience_metrics.stability.page_load_count";

// Number of times a renderer process crashed since the last report.
const char kStabilityRendererCrashCount[] =
    "user_experience_metrics.stability.renderer_crash_count";

// Number of times an extension renderer process crashed since the last report.
const char kStabilityExtensionRendererCrashCount[] =
    "user_experience_metrics.stability.extension_renderer_crash_count";

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

#if defined(OS_ANDROID)
// Activity type that is currently in the foreground for the UMA session.
// Uses the ActivityTypeIds::Type enum.
const char kStabilityForegroundActivityType[] =
    "user_experience_metrics.stability.current_foreground_activity_type";

// Tracks which Activities were launched during the last session.
// See |metrics_service_android.cc| for its usage.
const char kStabilityLaunchedActivityFlags[] =
    "user_experience_metrics.stability.launched_activity_flags";

// List pref: Counts how many times each Activity was launched.
// Indexed into by ActivityTypeIds::Type.
const char kStabilityLaunchedActivityCounts[] =
    "user_experience_metrics.stability.launched_activity_counts";

// List pref: Counts how many times each Activity type was in the foreground
// when a UMA session failed to be shut down properly.
// Indexed into by ActivityTypeIds::Type.
const char kStabilityCrashedActivityCounts[] =
    "user_experience_metrics.stability.crashed_activity_counts";
#endif

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
const char kUninstallLastLaunchTimeSec[] =
    "uninstall_metrics.last_launch_time_sec";
const char kUninstallLastObservedRunTimeSec[] =
    "uninstall_metrics.last_observed_running_time_sec";

// String containing the version of Chrome for which Chrome will not prompt the
// user about setting Chrome as the default browser.
const char kBrowserSuppressDefaultBrowserPrompt[] =
    "browser.suppress_default_browser_prompt_for_version";

// A collection of position, size, and other data relating to the browser
// window to restore on startup.
const char kBrowserWindowPlacement[] = "browser.window_placement";

// Browser window placement for popup windows.
const char kBrowserWindowPlacementPopup[] = "browser.window_placement_popup";

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

#if defined(OS_WIN)
// Whether downloaded PDFs should be opened in Adobe Acrobat Reader.
const char kOpenPdfDownloadInAdobeReader[] =
    "download.open_pdf_in_adobe_reader";
#endif

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

// Integer that specifies if the first run bubble should be shown.
// This preference is only registered by the first-run procedure.
const char kShowFirstRunBubbleOption[] = "show-first-run-bubble-option";

// String containing the last known intranet redirect URL, if any.  See
// intranet_redirect_detector.h for more information.
const char kLastKnownIntranetRedirectOrigin[] = "browser.last_redirect_origin";

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
// Preference to be used while relaunching Chrome. This preference dictates if
// Chrome should be launched in Metro or Desktop mode.
// For more info take a look at ChromeRelaunchMode enum.
const char kRelaunchMode[] = "relaunch.mode";
#endif

// Placeholder preference for disabling voice / video chat if it is ever added.
// Currently, this does not change any behavior.
const char kDisableVideoAndChat[] = "disable_video_chat";

// Whether Extensions are enabled.
const char kDisableExtensions[] = "extensions.disabled";

// Whether the plugin finder that lets you install missing plug-ins is enabled.
const char kDisablePluginFinder[] = "plugins.disable_plugin_finder";

// Customized app page names that appear on the New Tab Page.
const char kNtpAppPageNames[] = "ntp.app_page_names";

// Keeps track of currently open tabs collapsed state in the Other Devices menu.
const char kNtpCollapsedCurrentlyOpenTabs[] = "ntp.collapsed_open_tabs";

// Keeps track of which sessions are collapsed in the Other Devices menu.
const char kNtpCollapsedForeignSessions[] = "ntp.collapsed_foreign_sessions";

// Keeps track of recently closed tabs collapsed state in the Other Devices
// menu.
const char kNtpCollapsedRecentlyClosedTabs[] =
    "ntp.collapsed_recently_closed_tabs";

// Keeps track of snapshot documents collapsed state in the Other Devices menu.
const char kNtpCollapsedSnapshotDocument[] = "ntp.collapsed_snapshot_document";

// Keeps track of sync promo collapsed state in the Other Devices menu.
const char kNtpCollapsedSyncPromo[] = "ntp.collapsed_sync_promo";

// Serves dates to determine display of elements on the NTP.
const char kNtpDateResourceServer[] = "ntp.date_resource_server";

// New Tab Page URLs that should not be shown as most visited thumbnails.
const char kNtpMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

// True if a desktop sync session was found for this user.
const char kNtpPromoDesktopSessionFound[] = "ntp.promo_desktop_session_found";

// Last time of update of promo_resource_cache.
const char kNtpPromoResourceCacheUpdate[] = "ntp.promo_resource_cache_update";

// Which bookmarks folder should be visible on the new tab page v4.
const char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";

// Which page should be visible on the new tab page v4
const char kNtpShownPage[] = "ntp.shown_page";

// Serves tips for the NTP.
const char kNtpTipsResourceServer[] = "ntp.tips_resource_server";

// Boolean indicating whether the web store is active for the current locale.
const char kNtpWebStoreEnabled[] = "ntp.webstore_enabled";

// A private RSA key for ADB handshake.
const char kDevToolsAdbKey[] = "devtools.adb_key";

const char kDevToolsDisabled[] = "devtools.disabled";

// Determines whether devtools should be discovering usb devices for
// remote debugging at chrome://inspect.
const char kDevToolsDiscoverUsbDevicesEnabled[] =
    "devtools.discover_usb_devices";

// Maps of files edited locally using DevTools.
const char kDevToolsEditedFiles[] = "devtools.edited_files";

// List of file system paths added in DevTools.
const char kDevToolsFileSystemPaths[] = "devtools.file_system_paths";

// A boolean specifying whether dev tools window should be opened docked.
const char kDevToolsOpenDocked[] = "devtools.open_docked";

// A boolean specifying whether port forwarding should be enabled.
const char kDevToolsPortForwardingEnabled[] =
    "devtools.port_forwarding_enabled";

// A boolean specifying whether default port forwarding configuration has been
// set.
const char kDevToolsPortForwardingDefaultSet[] =
    "devtools.port_forwarding_default_set";

// A dictionary of port->location pairs for port forwarding.
const char kDevToolsPortForwardingConfig[] = "devtools.port_forwarding_config";

#if defined(OS_ANDROID)
// A boolean specifying whether remote dev tools debugging is enabled.
const char kDevToolsRemoteEnabled[] = "devtools.remote_enabled";
#endif

// Boolean indicating that TiclInvalidationService should use GCM channel.
// False or lack of settings means XMPPPushClient channel.
const char kInvalidationServiceUseGCMChannel[] =
    "invalidation_service.use_gcm_channel";

// Local hash of authentication password, used for off-line authentication
// when on-line authentication is not available.
const char kGoogleServicesPasswordHash[] = "google.services.password_hash";

#if !defined(OS_ANDROID)
// Tracks the number of times that we have shown the sign in promo at startup.
const char kSignInPromoStartupCount[] = "sync_promo.startup_count";

// Boolean tracking whether the user chose to skip the sign in promo.
const char kSignInPromoUserSkipped[] = "sync_promo.user_skipped";

// Boolean that specifies if the sign in promo is allowed to show on first run.
// This preference is specified in the master preference file to suppress the
// sign in promo for some installations.
const char kSignInPromoShowOnFirstRunAllowed[] =
    "sync_promo.show_on_first_run_allowed";

// Boolean that specifies if we should show a bubble in the new tab page.
// The bubble is used to confirm that the user is signed into sync.
const char kSignInPromoShowNTPBubble[] = "sync_promo.show_ntp_bubble";
#endif

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

// Boolean controlling whether curtaining is required when connecting to a host.
const char kRemoteAccessHostAllowClientPairing[] =
    "remote_access.host_allow_client_pairing";

// Whether Chrome Remote Desktop can proxy gnubby authentication traffic.
const char kRemoteAccessHostAllowGnubbyAuth[] =
    "remote_access.host_allow_gnubby_auth";

// Boolean that indicates whether the Chromoting host should allow connections
// using relay servers.
const char kRemoteAccessHostAllowRelayedConnection[] =
    "remote_access.host_allow_relayed_connection";

// String containing the UDP port range that the Chromoting host should used
// when connecting to clients. The port range should be in the form:
// <min_port>-<max_port>. E.g. 12400-12409.
const char kRemoteAccessHostUdpPortRange[] =
    "remote_access.host_udp_port_range";

// The last used printer and its settings.
const char kPrintPreviewStickySettings[] =
    "printing.print_preview_sticky_settings";

// The last requested size of the dialog as it was closed.
const char kCloudPrintDialogWidth[] = "cloud_print.dialog_size.width";
const char kCloudPrintDialogHeight[] = "cloud_print.dialog_size.height";
const char kCloudPrintSigninDialogWidth[] =
    "cloud_print.signin_dialog_size.width";
const char kCloudPrintSigninDialogHeight[] =
    "cloud_print.signin_dialog_size.height";

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

// Boolean that specifies whether the built-in asynchronous DNS client is used.
const char kBuiltInDnsClientEnabled[] = "async_dns.enabled";

// A pref holding the value of the policy used to explicitly allow or deny
// access to audio capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to audio capture devices
// is not allowed and no prompt will be shown.
// See also kAudioCaptureAllowedUrls.
const char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to audio
// capture devices without prompt.  NOTE: This whitelist is currently only
// supported when running in kiosk mode.
// TODO(tommi): Update comment when this is supported for all modes.
const char kAudioCaptureAllowedUrls[] = "hardware.audio_capture_allowed_urls";

// A pref holding the value of the policy used to explicitly allow or deny
// access to video capture devices.  When enabled or not set, the user is
// prompted for device access.  When disabled, access to video capture devices
// is not allowed and no prompt will be shown.
const char kVideoCaptureAllowed[] = "hardware.video_capture_enabled";
// Holds URL patterns that specify URLs that will be granted access to video
// capture devices without prompt.  NOTE: This whitelist is currently only
// supported when running in kiosk mode.
// TODO(tommi): Update comment when this is supported for all modes.
const char kVideoCaptureAllowedUrls[] = "hardware.video_capture_allowed_urls";

// A boolean pref that controls the enabled-state of hotword search voice
// trigger.
const char kHotwordSearchEnabled[] = "hotword.search_enabled_2";

// A boolean pref that controls whether the sound of "Ok, Google" plus a few
// seconds of audio data before is sent back to improve voice search.
const char kHotwordAudioLoggingEnabled[] = "hotword.audio_logging_enabled";

// A string holding the locale information under which Hotword was installed.
// It is used for comparison since the hotword voice search trigger must be
// reinstalled to handle a new language.
const char kHotwordPreviousLanguage[] = "hotword.previous_language";

#if defined(OS_ANDROID)
// Boolean that controls the global enabled-state of protected media identifier.
const char kProtectedMediaIdentifierEnabled[] =
    "protected_media_identifier.enabled";
#endif

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

// A pref holding the value of the policy used to disable mounting of external
// storage for the user.
const char kExternalStorageDisabled[] = "hardware.external_storage_disabled";

// A pref holding the value of the policy used to disable playing audio on
// ChromeOS devices. This pref overrides |kAudioMute| but does not overwrite
// it, therefore when the policy is lifted the original mute state is restored.
const char kAudioOutputAllowed[] = "hardware.audio_output_enabled";

// A dictionary that maps usernames to wallpaper properties.
const char kUsersWallpaperInfo[] = "user_wallpaper_info";

// Copy of owner swap mouse buttons option to use on login screen.
const char kOwnerPrimaryMouseButtonRight[] = "owner.mouse.primary_right";

// Copy of owner tap-to-click option to use on login screen.
const char kOwnerTapToClickEnabled[] = "owner.touchpad.enable_tap_to_click";

// The length of device uptime after which an automatic reboot is scheduled,
// expressed in seconds.
const char kUptimeLimit[] = "automatic_reboot.uptime_limit";

// Whether an automatic reboot should be scheduled when an update has been
// applied and a reboot is required to complete the update process.
const char kRebootAfterUpdate[] = "automatic_reboot.reboot_after_update";

// An any-api scoped refresh token for enterprise-enrolled devices.  Allows
// for connection to Google APIs when the user isn't logged in.  Currently used
// for for getting a cloudprint scoped token to allow printing in Guest mode,
// Public Accounts and kiosks.
const char kDeviceRobotAnyApiRefreshToken[] =
    "device_robot_refresh_token.any-api";

// Device requisition for enterprise enrollment.
const char kDeviceEnrollmentRequisition[] = "enrollment.device_requisition";

// Whether to automatically start the enterprise enrollment step during OOBE.
const char kDeviceEnrollmentAutoStart[] = "enrollment.auto_start";

// Whether the user may exit enrollment.
const char kDeviceEnrollmentCanExit[] = "enrollment.can_exit";

// How many times HID detection OOBE dialog was shown.
const char kTimesHIDDialogShown[] = "HIDDialog.shown_how_many_times";

// Dictionary of per-user Least Recently Used input method (used at login
// screen).
extern const char kUsersLRUInputMethod[] = "UsersLRUInputMethod";

// A dictionary pref of the echo offer check flag. It sets offer info when
// an offer is checked.
extern const char kEchoCheckedOffers[] = "EchoCheckedOffers";

// Key name of a dictionary in local state to store cached multiprofle user
// behavior policy value.
const char kCachedMultiProfileUserBehavior[] = "CachedMultiProfileUserBehavior";

// A string pref with initial locale set in VPD or manifest.
const char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the OOBE complete flag (first OOBE part before login).
const char kOobeComplete[] = "OobeComplete";

// The name of the screen that has to be shown if OOBE has been interrupted.
const char kOobeScreenPending[] = "OobeScreenPending";

// A boolean pref of the device registered flag (second part after first login).
const char kDeviceRegistered[] = "DeviceRegistered";

// Boolean pref to signal corrupted enrollment to force the device through
// enrollment recovery flow upon next boot.
const char kEnrollmentRecoveryRequired[] = "EnrollmentRecoveryRequired";

// List of usernames that used certificates pushed by policy before.
// This is used to prevent these users from joining multiprofile sessions.
const char kUsedPolicyCertificates[] = "policy.used_policy_certificates";

// A dictionary containing server-provided device state pulled form the cloud
// after recovery.
const char kServerBackedDeviceState[] = "server_backed_device_state";

// Customized wallpaper URL, which is already downloaded and scaled.
// The URL from this preference must never be fetched. It is compared to the
// URL from customization document to check if wallpaper URL has changed
// since wallpaper was cached.
const char kCustomizationDefaultWallpaperURL[] =
    "customization.default_wallpaper_url";

// System uptime, when last logout started.
// This is saved to file and cleared after chrome process starts.
const char kLogoutStartedLast[] = "chromeos.logout-started";

// An integer pref of the current consumer management enrollment state. The
// meaning of the value is defined in the enum EnrollmentState in:
//   chrome/browser/chromeos/policy/consumer_management_service.h
const char kConsumerManagementEnrollmentState[] =
    "consumer_management.enrollment_state";
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

const char kPerformanceTracingEnabled[] =
    "feedback.performance_tracing_enabled";

// Boolean indicating whether tabstrip uses stacked layout (on touch devices).
// Defaults to false.
const char kTabStripStackedLayout[] = "tab-strip-stacked-layout";

// Indicates that factory reset was requested from options page or reset screen.
const char kFactoryResetRequested[] = "FactoryResetRequested";

// Indicates that rollback was requested alongside with factory reset.
// Makes sense only if kFactoryResetRequested is true.
const char kRollbackRequested[] = "RollbackRequested";

// Boolean recording whether we have showed a balloon that calls out the message
// center for desktop notifications.
const char kMessageCenterShowedFirstRunBalloon[] =
    "message_center.showed_first_run_balloon";

// Boolean recording whether the user has disabled the notifications
// menubar or systray icon.
const char kMessageCenterShowIcon[] = "message_center.show_icon";

const char kMessageCenterForcedOnTaskbar[] =
    "message_center.was_forced_on_taskbar";

// *************** SERVICE PREFS ***************
// These are attached to the service process.

const char kCloudPrintRoot[] = "cloud_print";
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
// A boolean indicating whether we should connect to cloud print new printers.
const char kCloudPrintConnectNewPrinters[] =
    "cloud_print.user_settings.connectNewPrinters";
// A boolean indicating whether we should ping XMPP connection.
const char kCloudPrintXmppPingEnabled[] = "cloud_print.xmpp_ping_enabled";
// An int value indicating the average timeout between xmpp pings.
const char kCloudPrintXmppPingTimeout[] = "cloud_print.xmpp_ping_timeout_sec";
// Dictionary with settings stored by connector setup page.
const char kCloudPrintUserSettings[] = "cloud_print.user_settings";
// List of printers settings.
extern const char kCloudPrintPrinters[] = "cloud_print.user_settings.printers";
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

#if defined(OS_MACOSX)
// Set to true if the user removed our login item so we should not create a new
// one when uninstalling background apps.
const char kUserRemovedLoginItem[] = "background_mode.user_removed_login_item";

// Set to true if Chrome already created a login item, so there's no need to
// create another one.
const char kChromeCreatedLoginItem[] =
  "background_mode.chrome_created_login_item";

// Set to true once we've initialized kChromeCreatedLoginItem for the first
// time.
const char kMigratedLoginItemPref[] =
  "background_mode.migrated_login_item_pref";

// A boolean that tracks whether to show a notification when trying to quit
// while there are apps running.
const char kNotifyWhenAppsKeepChromeAlive[] =
    "apps.notify-when-apps-keep-chrome-alive";
#endif

// Set to true if background mode is enabled on this browser.
const char kBackgroundModeEnabled[] = "background_mode.enabled";

// Set to true if hardware acceleration mode is enabled on this browser.
const char kHardwareAccelerationModeEnabled[] =
  "hardware_acceleration_mode.enabled";

// Hardware acceleration mode from previous browser launch.
const char kHardwareAccelerationModePrevious[] =
  "hardware_acceleration_mode_previous";

// List of protocol handlers.
const char kRegisteredProtocolHandlers[] =
  "custom_handlers.registered_protocol_handlers";

// List of protocol handlers the user has requested not to be asked about again.
const char kIgnoredProtocolHandlers[] =
  "custom_handlers.ignored_protocol_handlers";

// List of protocol handlers registered by policy.
const char kPolicyRegisteredProtocolHandlers[] =
    "custom_handlers.policy.registered_protocol_handlers";

// List of protocol handlers the policy has requested to be ignored.
const char kPolicyIgnoredProtocolHandlers[] =
    "custom_handlers.policy.ignored_protocol_handlers";

// Whether user-specified handlers for protocols and content types can be
// specified.
const char kCustomHandlersEnabled[] = "custom_handlers.enabled";

// Integer that specifies the policy refresh rate for device-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
const char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";

// A boolean where true means that the browser has previously attempted to
// enable autoupdate and failed, so the next out-of-date browser start should
// not prompt the user to enable autoupdate, it should offer to reinstall Chrome
// instead.
const char kAttemptedToEnableAutoupdate[] =
    "browser.attempted_to_enable_autoupdate";

// The next media gallery ID to assign.
const char kMediaGalleriesUniqueId[] = "media_galleries.gallery_id";

// A list of dictionaries, where each dictionary represents a known media
// gallery.
const char kMediaGalleriesRememberedGalleries[] =
    "media_galleries.remembered_galleries";

// The last time a media scan completed.
const char kMediaGalleriesLastScanTime[] = "media_galleries.last_scan_time";

#if defined(USE_ASH)
// |kShelfAlignment| and |kShelfAutoHideBehavior| have a local variant. The
// local variant is not synced and is used if set. If the local variant is not
// set its value is set from the synced value (once prefs have been
// synced). This gives a per-machine setting that is initialized from the last
// set value.
// These values are default on the machine but can be overridden by per-display
// values in kShelfPreferences (unless overridden by managed policy).
// String value corresponding to ash::Shell::ShelfAlignment.
const char kShelfAlignment[] = "shelf_alignment";
const char kShelfAlignmentLocal[] = "shelf_alignment_local";
// String value corresponding to ash::Shell::ShelfAutoHideBehavior.
const char kShelfAutoHideBehavior[] = "auto_hide_behavior";
const char kShelfAutoHideBehaviorLocal[] = "auto_hide_behavior_local";
// This value stores chrome icon's index in the launcher. This should be handled
// separately with app shortcut's index because of ShelfModel's backward
// compatibility. If we add chrome icon index to |kPinnedLauncherApps|, its
// index is also stored in the |kPinnedLauncherApp| pref. It may causes
// creating two chrome icons.
const char kShelfChromeIconIndex[] = "shelf_chrome_icon_index";
// Dictionary value that holds per-display preference of shelf alignment and
// auto-hide behavior. Key of the dictionary is the id of the display, and
// its value is a dictionary whose keys are kShelfAlignment and
// kShelfAutoHideBehavior.
const char kShelfPreferences[] = "shelf_preferences";

// Integer value in milliseconds indicating the length of time for which a
// confirmation dialog should be shown when the user presses the logout button.
// A value of 0 indicates that logout should happen immediately, without showing
// a confirmation dialog.
const char kLogoutDialogDurationMs[] = "logout_dialog_duration_ms";
const char kPinnedLauncherApps[] = "pinned_launcher_apps";
// Boolean value indicating whether to show a logout button in the ash tray.
const char kShowLogoutButtonInTray[] = "show_logout_button_in_tray";
#endif

#if defined(USE_AURA)
// Tuning settings for gestures.
const char kMaxSeparationForGestureTouchesInPixels[] =
    "gesture.max_separation_for_gesture_touches_in_pixels";
const char kSemiLongPressTimeInSeconds[] =
    "gesture.semi_long_press_time_in_seconds";
const char kTabScrubActivationDelayInMS[] =
    "gesture.tab_scrub_activation_delay_in_ms";
const char kFlingAccelerationCurveCoefficient0[] =
    "gesture.fling_acceleration_curve_coefficient_0";
const char kFlingAccelerationCurveCoefficient1[] =
    "gesture.fling_acceleration_curve_coefficient_1";
const char kFlingAccelerationCurveCoefficient2[] =
    "gesture.fling_acceleration_curve_coefficient_2";
const char kFlingAccelerationCurveCoefficient3[] =
    "gesture.fling_acceleration_curve_coefficient_3";
const char kFlingCurveTouchpadAlpha[] = "flingcurve.touchpad_alpha";
const char kFlingCurveTouchpadBeta[] = "flingcurve.touchpad_beta";
const char kFlingCurveTouchpadGamma[] = "flingcurve.touchpad_gamma";
const char kFlingCurveTouchscreenAlpha[] = "flingcurve.touchscreen_alpha";
const char kFlingCurveTouchscreenBeta[] = "flingcurve.touchscreen_beta";
const char kFlingCurveTouchscreenGamma[] = "flingcurve.touchscreen_gamma";
const char kFlingMaxCancelToDownTimeInMs[] =
    "gesture.fling_max_cancel_to_down_time_in_ms";
const char kFlingMaxTapGapTimeInMs[] =
    "gesture.fling_max_tap_gap_time_in_ms";
const char kOverscrollHorizontalThresholdComplete[] =
    "overscroll.horizontal_threshold_complete";
const char kOverscrollVerticalThresholdComplete[] =
    "overscroll.vertical_threshold_complete";
const char kOverscrollMinimumThresholdStart[] =
    "overscroll.minimum_threshold_start";
const char kOverscrollMinimumThresholdStartTouchpad[] =
    "overscroll.minimum_threshold_start_touchpad";
const char kOverscrollVerticalThresholdStart[] =
    "overscroll.vertical_threshold_start";
const char kOverscrollHorizontalResistThreshold[] =
    "overscroll.horizontal_resist_threshold";
const char kOverscrollVerticalResistThreshold[] =
    "overscroll.vertical_resist_threshold";
#endif

// Counts how many more times the 'profile on a network share' warning should be
// shown to the user before the next silence period.
const char kNetworkProfileWarningsLeft[] = "network_profile.warnings_left";
// Tracks the time of the last shown warning. Used to reset
// |network_profile.warnings_left| after a silence period.
const char kNetworkProfileLastWarningTime[] =
    "network_profile.last_warning_time";

#if defined(OS_CHROMEOS)
// The RLZ brand code, if enabled.
const char kRLZBrand[] = "rlz.brand";
// Whether RLZ pings are disabled.
const char kRLZDisabled[] = "rlz.disabled";
#endif

#if defined(ENABLE_APP_LIST)
// The directory in user data dir that contains the profile to be used with the
// app launcher.
const char kAppListProfile[] = "app_list.profile";

// The number of times the app launcher was launched since last ping and
// the time of the last ping.
const char kAppListLaunchCount[] = "app_list.launch_count";
const char kLastAppListLaunchPing[] = "app_list.last_launch_ping";

// The number of times the an app was launched from the app launcher since last
// ping and the time of the last ping.
const char kAppListAppLaunchCount[] = "app_list.app_launch_count";
const char kLastAppListAppLaunchPing[] = "app_list.last_app_launch_ping";

// A boolean that tracks whether the user has ever enabled the app launcher.
const char kAppLauncherHasBeenEnabled[] =
    "apps.app_launcher.has_been_enabled";

// An enum indicating how the app launcher was enabled. E.g., via webstore, app
// install, command line, etc. For UMA.
const char kAppListEnableMethod[] = "app_list.how_enabled";

// The time that the app launcher was enabled. Cleared when UMA is recorded.
const char kAppListEnableTime[] = "app_list.when_enabled";

// TODO(calamity): remove this pref since app launcher will always be
// installed.
// Local state caching knowledge of whether the app launcher is installed.
const char kAppLauncherIsEnabled[] =
    "apps.app_launcher.should_show_apps_page";

// Integer representing the version of the app launcher shortcut installed on
// the system. Incremented, e.g., when embedded icons change.
const char kAppLauncherShortcutVersion[] = "apps.app_launcher.shortcut_version";

// A boolean identifying if we should show the app launcher promo or not.
const char kShowAppLauncherPromo[] = "app_launcher.show_promo";

// A dictionary that tracks the Drive app to Chrome app mapping. The key is
// a Drive app id and the value is the corresponding Chrome app id. The pref
// is unsynable and used to track local mappings only.
const char kAppLauncherDriveAppMapping[] =
    "apps.app_launcher.drive_app_mapping";
#endif

// If set, the user requested to launch the app with this extension id while
// in Metro mode, and then relaunched to Desktop mode to start it.
const char kAppLaunchForMetroRestart[] = "apps.app_launch_for_metro_restart";

// Set with |kAppLaunchForMetroRestart|, the profile whose loading triggers
// launch of the specified app when restarting Chrome in desktop mode.
const char kAppLaunchForMetroRestartProfile[] =
    "apps.app_launch_for_metro_restart_profile";

// An integer that is incremented whenever changes are made to app shortcuts.
// Increasing this causes all app shortcuts to be recreated.
const char kAppShortcutsVersion[] = "apps.shortcuts_version";

// How often the bubble has been shown.
extern const char kModuleConflictBubbleShown[] = "module_conflict.bubble_shown";

// A string pref for storing the salt used to compute the pepper device ID.
const char kDRMSalt[] = "settings.privacy.drm_salt";
// A boolean pref that enables the (private) pepper GetDeviceID() call and
// enables the use of remote attestation for content protection.
const char kEnableDRM[] = "settings.privacy.drm_enabled";

// An integer per-profile pref that signals if the watchdog extension is
// installed and active. We need to know if the watchdog extension active for
// ActivityLog initialization before the extension system is initialized.
const char kWatchdogExtensionActive[] =
    "profile.extensions.activity_log.num_consumers_active";
// The old version was a bool.
const char kWatchdogExtensionActiveOld[] =
    "profile.extensions.activity_log.watchdog_extension_active";

#if defined(OS_ANDROID)
// A list of partner bookmark rename/remove mappings.
// Each list item is a dictionary containing a "url", a "provider_title" and
// "mapped_title" entries, detailing the bookmark target URL (if any), the title
// given by the PartnerBookmarksProvider and either the user-visible renamed
// title or an empty string if the bookmark node was removed.
const char kPartnerBookmarkMappings[] = "partnerbookmarks.mappings";
#endif

// Whether DNS Quick Check is disabled in proxy resolution.
const char kQuickCheckEnabled[] = "proxy.quick_check_enabled";

}  // namespace prefs
