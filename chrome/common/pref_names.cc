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

// This is one of three integer values:
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

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const wchar_t kShowBookmarkBar[] = L"bookmark_bar.show_on_all_tabs";

// Boolean that is true if the password manager is on (will record new
// passwords and fill in known passwords).
const wchar_t kPasswordManagerEnabled[] = L"profile.password_manager_enabled";

// Boolean that is true if the form autofill is on (will record values entered
// in text inputs in forms and shows them in a popup when user type in a text
// input with the same name later on).
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

// Boolean that is true if mixed content should be filtered.
// TODO(jcampan): http://b/1084034: at some point this will become an enum
//                 (int): don't filter, filter everything, filter images only.
const wchar_t kMixedContentFiltering[] = L"security.mixed_content_filtering";

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
// sub-resource hostnames (and expected latency benefits from pre-resolving such
// sub-resource hostnames).
// This list is adaptively grown and pruned.
extern const wchar_t kDnsHostReferralList[] = L"HostReferralList";

#if defined(OS_LINUX)
// Prefs for SSLConfigServicePref.  Currently, these are only present on
// and used by Linux.
extern const wchar_t kCertRevocationCheckingEnabled[] =
    L"ssl.rev_checking.enabled";
extern const wchar_t kSSL2Enabled[] = L"ssl.ssl2.enabled";
extern const wchar_t kSSL3Enabled[] = L"ssl.ssl3.enabled";
extern const wchar_t kTLS1Enabled[] = L"ssl.tls1.enabled";
#endif

#if defined(OS_CHROMEOS)
// A string pref set to the timezone.
extern const wchar_t kTimeZone[] = L"settings.datetime.timezone";

// A boolean pref set to true if TapToClick is being done in browser.
extern const wchar_t kTapToClickEnabled[] =
    L"settings.touchpad.enable_tap_to_click";

// A boolean pref set to true if VertEdgeScroll is being done in browser.
extern const wchar_t kVertEdgeScrollEnabled[] =
    L"settings.touchpad.enable_vert_edge_scroll";

// A integer pref for the touchpad speed factor.
extern const wchar_t kTouchpadSpeedFactor[] = L"settings.touchpad.speed_factor";

// A integer pref for the touchpad sensitivity.
extern const wchar_t kTouchpadSensitivity[] = L"settings.touchpad.sensitivity";
#endif

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

// Integer prefs giving the widths of the columns in the bookmark table. Two
// configurations are saved, one with the path column and one without.
const wchar_t kBookmarkTableNameWidth1[] = L"bookmark_table.name_width_1";
const wchar_t kBookmarkTableURLWidth1[] = L"bookmark_table.url_width_1";
const wchar_t kBookmarkTableNameWidth2[] = L"bookmark_table.name_width_2";
const wchar_t kBookmarkTableURLWidth2[] = L"bookmark_table.url_width_2";
const wchar_t kBookmarkTablePathWidth[] = L"bookmark_table.path_width";

// Bounds of the bookmark manager.
const wchar_t kBookmarkManagerPlacement[] =
    L"bookmark_manager.window_placement";

// Integer location of the split bar in the bookmark manager.
const wchar_t kBookmarkManagerSplitLocation[] =
    L"bookmark_manager.split_location";

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
#if defined(OS_LINUX)
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

// Boolean that indicates whether the infobar explaining that search can be done
// directly from the omnibox should be shown.
const wchar_t kShowOmniboxSearchHint[] = L"browser.show_omnibox_search_hint";

// Integer that counts the number of times the promo on the NTP has left to be
// shown; this decrements each time the NTP is shown for the first time
// in a session. The message line can be closed separately from the promo
// image, so we store two separate values for number of remaining views.
const wchar_t kNTPPromoLineRemaining[] = L"browser.ntp.promo_line_remaining";
const wchar_t kNTPPromoImageRemaining[] = L"browser.ntp.promo_image_remaining";

// The list of origins which are allowed|denied to show desktop notifications.
const wchar_t kDesktopNotificationAllowedOrigins[] =
    L"profile.notification_allowed_sites";
const wchar_t kDesktopNotificationDeniedOrigins[] =
    L"profile.notification_denied_sites";

// Dictionary of content settings applied to all hosts by default.
const wchar_t kDefaultContentSettings[] = L"profile.default_content_settings";

// Dictionary that maps hostnames to content related settings.  Default
// settings will be applied to hosts not in this pref.
const wchar_t kPerHostContentSettings[] = L"profile.per_host_content_settings";

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const wchar_t kBlockThirdPartyCookies[] = L"profile.block_third_party_cookies";

// Boolean that is true when all locally stored site data (e.g. cookies, local
// storage, etc..) should be deleted on exit.
const wchar_t kClearSiteDataOnExit[] = L"profile.clear_site_data_on_exit";

// Dictionary that maps hostnames to zoom levels.  Hosts not in this pref will
// be displayed at the default zoom level.
const wchar_t kPerHostZoomLevels[] = L"profile.per_host_zoom_levels";

// Boolean that is true if the autofill infobar has been shown to the user.
const wchar_t kAutoFillInfoBarShown[] = L"autofill.infobar_shown";

// Boolean that is true if autofill is enabled and allowed to save profile data.
const wchar_t kAutoFillEnabled[] = L"autofill.enabled";

// Dictionary that maps providers to lists of filter rules.
const wchar_t kPrivacyFilterRules[] = L"profile.privacy_filter_rules";

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
extern const wchar_t kStabilityStatsBuildTime[] =
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

// Number of milliseconds that the main application process was up since
// the last report.
const wchar_t kStabilityUptimeSec[] =
    L"user_experience_metrics.stability.uptime_sec";

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

// The mere fact that this pref is registered signals that we should show the
// First Run Search Information bubble when the first browser window appears.
// This preference is only registered by the first-run procedure.
const wchar_t kShouldShowFirstRunBubble[] = L"show-first-run-bubble";

// The mere fact that this pref is registered signals that we should show the
// smaller OEM First Run Search Information bubble when the first
// browser window appears.
// This preference is only registered by the first-run procedure.
const wchar_t kShouldUseOEMFirstRunBubble[] = L"show-OEM-first-run-bubble";

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

// Create web application shortcut dialog preferences.
const wchar_t kWebAppCreateOnDesktop[] = L"browser.web_app.create_on_desktop";
const wchar_t kWebAppCreateInAppsMenu[] =
    L"browser.web_app.create_in_apps_menu";
const wchar_t kWebAppCreateInQuickLaunchBar[] =
    L"browser.web_app.create_in_quick_launch_bar";

}  // namespace prefs
