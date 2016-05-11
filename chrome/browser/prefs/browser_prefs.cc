// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/recovery_component_installer.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/geolocation/geolocation_prefs.h"
#include "chrome/browser/gpu/gl_string_manager.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/net/http_server_properties_manager_factory.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/notifications/extension_welcome_notification.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/background_budget_service.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/plugins/plugins_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/metrics/metrics_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/rappor/rappor_service.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "components/web_resource/promo_resource_service.h"
#include "content/public/browser/render_process_host.h"
#include "net/http/http_server_properties_manager.h"

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/drive/drive_app_mapping.h"
#include "chrome/browser/apps/shortcut_manager.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/copresence/copresence_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/component_migration_helper.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "extensions/browser/extension_prefs.h"
#endif  // defined(ENABLE_EXTENSIONS)

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/ntp/most_visited_sites.h"
#include "chrome/browser/android/ntp/new_tab_page_prefs.h"
#include "chrome/browser/android/ntp/popular_sites.h"
#else
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/upgrade_detector.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/file_system_provider/registry.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_mode_detector.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/quirks/quirks_manager.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_CHROMEOS) && defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/google_now_extension.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/apps/app_launch_for_metro_restart_win.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

namespace {

#if defined(OS_WIN)
// Deprecated 11/2015 (M48). TODO(gab): delete in M52+.
const char kShownAutoLaunchInfobarDeprecated[] =
    "browser.shown_autolaunch_infobar";
#endif  // defined(OS_WIN)

// The SessionStartupPref used this pref to store the list of URLs to restore
// on startup, and then renamed it to "sessions.startup_urls" in M31. Migration
// code was added and the timestamp of when the migration happened was tracked
// by "session.startup_urls_migration_time". Both are obsolete now (12/2015) and
// should be removed once a few releases have happened.
const char kURLsToRestoreOnStartupOld[] = "session.urls_to_restore_on_startup";
const char kRestoreStartupURLsMigrationTime[] =
  "session.startup_urls_migration_time";

// Deprecated 12/2015.
const char kRestoreOnStartupMigrated[] = "session.restore_on_startup_migrated";

#if defined(USE_AURA)
// Deprecated 1/2016.
const char kMaxSeparationForGestureTouchesInPixels[] =
    "gesture.max_separation_for_gesture_touches_in_pixels";
const char kSemiLongPressTimeInMs[] = "gesture.semi_long_press_time_in_ms";
const char kTabScrubActivationDelayInMs[] =
    "gesture.tab_scrub_activation_delay_in_ms";
const char kFlingMaxCancelToDownTimeInMs[] =
    "gesture.fling_max_cancel_to_down_time_in_ms";
const char kFlingMaxTapGapTimeInMs[] = "gesture.fling_max_tap_gap_time_in_ms";
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
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_GOOGLE_NOW)
// Deprecated 3/2016
const char kGoogleGeolocationAccessEnabled[] =
    "googlegeolocationaccess.enabled";
#endif

// Deprecated 4/2016.
const char kCheckDefaultBrowser[] = "browser.check_default_browser";

}  // namespace

namespace chrome {

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Please keep this list alphabetized.
  AppListService::RegisterPrefs(registry);
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  chrome_prefs::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
  component_updater::RegisterPrefsForRecoveryComponent(registry);
  component_updater::SupervisedUserWhitelistInstaller::RegisterPrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  geolocation::RegisterPrefs(registry);
  GLStringManager::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  IOThread::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  profiles::RegisterPrefs(registry);
  rappor::RapporService::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  SigninManagerFactory::RegisterPrefs(registry);
  ssl_config::SSLConfigServiceManager::RegisterPrefs(registry);
  startup_metric_utils::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  web_resource::PromoResourceService::RegisterPrefs(registry);

  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);

#if defined(ENABLE_EXTENSIONS)
  EasyUnlockService::RegisterPrefs(registry);
#endif

#if defined(ENABLE_PLUGINS)
  PluginFinder::RegisterPrefs(registry);
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginsResourceService::RegisterPrefs(registry);
#endif

#if defined(ENABLE_TASK_MANAGER)
  TaskManager::RegisterPrefs(registry);
#endif  // defined(ENABLE_TASK_MANAGER)

#if BUILDFLAG(ENABLE_BACKGROUND)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  ChromeTracingDelegate::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  // The native GCM is used on Android instead.
  gcm::GCMChannelStatusSyncer::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
#if !defined(OS_CHROMEOS)
  RegisterDefaultBrowserPromptPrefs(registry);
#endif  // !defined(OS_CHROMEOS)
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  chromeos::ChromeUserManagerImpl::RegisterPrefs(registry);
  chromeos::DataPromoNotification::RegisterPrefs(registry);
  chromeos::DeviceOAuth2TokenService::RegisterPrefs(registry);
  chromeos::device_settings_cache::RegisterPrefs(registry);
  chromeos::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  chromeos::language_prefs::RegisterPrefs(registry);
  chromeos::KioskAppManager::RegisterPrefs(registry);
  chromeos::MultiProfileUserController::RegisterPrefs(registry);
  chromeos::HIDDetectionScreenHandler::RegisterPrefs(registry);
  chromeos::DemoModeDetector::RegisterPrefs(registry);
  chromeos::Preferences::RegisterPrefs(registry);
  chromeos::proxy_config::RegisterPrefs(registry);
  chromeos::RegisterDisplayLocalStatePrefs(registry);
  chromeos::ResetScreenHandler::RegisterPrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterPrefs(registry);
  chromeos::SigninScreenHandler::RegisterPrefs(registry);
  chromeos::StartupUtils::RegisterPrefs(registry);
  chromeos::system::AutomaticRebootManager::RegisterPrefs(registry);
  chromeos::system::InputDeviceSettings::RegisterPrefs(registry);
  chromeos::TimeZoneResolver::RegisterPrefs(registry);
  chromeos::UserImageManager::RegisterPrefs(registry);
  chromeos::UserSessionManager::RegisterPrefs(registry);
  chromeos::WallpaperManager::RegisterPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  extensions::ExtensionAssetsManagerChromeOS::RegisterPrefs(registry);
  invalidation::InvalidatorStorage::RegisterPrefs(registry);
  policy::AutoEnrollmentClient::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorChromeOS::RegisterPrefs(registry);
  policy::ConsumerManagementService::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
#endif

#if defined(OS_MACOSX)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
#endif

#if defined(OS_WIN)
  app_metro_launch::RegisterPrefs(registry);
  component_updater::RegisterPrefsForSwReporter(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewLocalPrefs(registry);
#endif
}

// Register prefs applicable to all profiles.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  TRACE_EVENT0("browser", "chrome::RegisterProfilePrefs");
  SCOPED_UMA_HISTOGRAM_TIMER("Settings.RegisterProfilePrefsTime");
  // User prefs. Please keep this list alphabetized.
  autofill::AutofillManager::RegisterProfilePrefs(registry);
  BackgroundBudgetService::RegisterProfilePrefs(registry);
  sync_driver::SyncPrefs::RegisterProfilePrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::HttpServerPropertiesManagerFactory::RegisterProfilePrefs(
      registry);
  chrome_browser_net::Predictor::RegisterProfilePrefs(registry);
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  InstantUI::RegisterProfilePrefs(registry);
  NavigationCorrectionTabObserver::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaStreamDevicesController::RegisterProfilePrefs(registry);
  NetPrefObserver::RegisterProfilePrefs(registry);
  ntp_snippets::NTPSnippetsService::RegisterProfilePrefs(registry);
  password_bubble_experiment::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

#if defined(ENABLE_APP_LIST)
  app_list::AppListPrefs::RegisterProfilePrefs(registry);
#endif

  policy::URLBlacklistManager::RegisterProfilePrefs(registry);

#if defined(ENABLE_EXTENSIONS)
  EasyUnlockService::RegisterProfilePrefs(registry);
  ExtensionWebUI::RegisterProfilePrefs(registry);
  RegisterAnimationPolicyPrefs(registry);
  ToolbarActionsBar::RegisterProfilePrefs(registry);
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::ComponentMigrationHelper::RegisterPrefs(registry);
  extensions::CopresenceService::RegisterProfilePrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::launch_util::RegisterProfilePrefs(registry);
#endif  // defined(ENABLE_EXTENSIONS)

#if defined(ENABLE_NOTIFICATIONS)
  NotifierStateTracker::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_NOTIFICATIONS) && defined(ENABLE_EXTENSIONS) && \
    !defined(OS_ANDROID)
  // The extension welcome notification requires a build that enables extensions
  // and notifications, and uses the UI message center.
  ExtensionWelcomeNotification::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_PLUGINS)
  PluginsUI::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_PRINT_PREVIEW)
  printing::StickySettings::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
  LocalDiscoveryUI::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#if !defined(OS_ANDROID)
  SupervisedUserSharedSettingsService::RegisterProfilePrefs(registry);
  SupervisedUserSyncService::RegisterProfilePrefs(registry);
#endif
  ChildAccountService::RegisterProfilePrefs(registry);
  SupervisedUserService::RegisterProfilePrefs(registry);
  SupervisedUserWhitelistService::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
  variations::VariationsService::RegisterProfilePrefs(registry);
  MostVisitedSites::RegisterProfilePrefs(registry);
  NewTabPagePrefs::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  PopularSites::RegisterProfilePrefs(registry);
#else
  AppShortcutManager::RegisterProfilePrefs(registry);
  DeviceIDFetcher::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  DriveAppMapping::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::ExtensionSettingsHandler::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  PepperFlashSettingsManager::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  arc::ArcAuthService::RegisterProfilePrefs(registry);
  chromeos::first_run::RegisterProfilePrefs(registry);
  chromeos::file_system_provider::RegisterProfilePrefs(registry);
  chromeos::KeyPermissions::RegisterProfilePrefs(registry);
  chromeos::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  chromeos::proxy_config::RegisterProfilePrefs(registry);
  chromeos::SAMLOfflineSigninLimiter::RegisterProfilePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::UserImageSyncObserver::RegisterProfilePrefs(registry);
  extensions::EPKPChallengeUserKey::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS) && defined(ENABLE_APP_LIST)
  ArcAppListPrefs::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN)
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewProfilePrefs(registry);
  RegisterInvertBubbleUserPrefs(registry);
#endif

#if defined(USE_ASH)
  ash::RegisterChromeLauncherUserPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
#endif

  // Preferences registered only for migration (clearing or moving to a new key)
  // go here.

#if defined(OS_WIN)
  registry->RegisterIntegerPref(kShownAutoLaunchInfobarDeprecated, 0);
#endif  // defined(OS_WIN)

#if defined(USE_AURA)
  registry->RegisterIntegerPref(kFlingMaxCancelToDownTimeInMs, 0);
  registry->RegisterIntegerPref(kFlingMaxTapGapTimeInMs, 0);
  registry->RegisterIntegerPref(kTabScrubActivationDelayInMs, 0);
  registry->RegisterIntegerPref(kSemiLongPressTimeInMs, 0);
  registry->RegisterDoublePref(kMaxSeparationForGestureTouchesInPixels, 0);

  registry->RegisterDoublePref(kOverscrollHorizontalThresholdComplete, 0);
  registry->RegisterDoublePref(kOverscrollVerticalThresholdComplete, 0);
  registry->RegisterDoublePref(kOverscrollMinimumThresholdStart, 0);
  registry->RegisterDoublePref(kOverscrollMinimumThresholdStartTouchpad, 0);
  registry->RegisterDoublePref(kOverscrollVerticalThresholdStart, 0);
  registry->RegisterDoublePref(kOverscrollHorizontalResistThreshold, 0);
  registry->RegisterDoublePref(kOverscrollVerticalResistThreshold, 0);
#endif  // defined(USE_AURA)

  registry->RegisterListPref(kURLsToRestoreOnStartupOld);
  registry->RegisterInt64Pref(kRestoreStartupURLsMigrationTime, 0);
  registry->RegisterBooleanPref(kRestoreOnStartupMigrated, false);

#if BUILDFLAG(ENABLE_GOOGLE_NOW)
  registry->RegisterBooleanPref(kGoogleGeolocationAccessEnabled, false);
#endif

  registry->RegisterBooleanPref(kCheckDefaultBrowser, true);
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);

#if defined(OS_CHROMEOS)
  chromeos::PowerPrefs::RegisterUserProfilePrefs(registry);
#endif
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

#if defined(OS_CHROMEOS)
void RegisterLoginProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);

  chromeos::PowerPrefs::RegisterLoginProfilePrefs(registry);
}
#endif

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteBrowserPrefs(Profile* profile, PrefService* local_state) {
  // Added 05/2016.
  web_resource::PromoResourceService::ClearLocalState(local_state);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  PrefService* profile_prefs = profile->GetPrefs();

#if defined(OS_CHROMEOS) && defined(ENABLE_APP_LIST)
  // Added 02/2015.
  MigrateGoogleNowPrefs(profile);
#endif

#if defined(OS_WIN)
  // Added 11/2015.
  profile_prefs->ClearPref(kShownAutoLaunchInfobarDeprecated);
#endif

#if defined(OS_MACOSX)
  // Migrate the value of kHideFullscreenToolbar to kShowFullscreenToolbar if
  // it was set by the user. See crbug.com/590827.
  // Added 03/2016.
  const PrefService::Preference* hide_pref =
      profile_prefs->FindPreference(prefs::kHideFullscreenToolbar);
  if (!hide_pref->IsDefaultValue()) {
    bool hide_pref_value =
        profile_prefs->GetBoolean(prefs::kHideFullscreenToolbar);
    profile_prefs->SetBoolean(prefs::kShowFullscreenToolbar, !hide_pref_value);
    profile_prefs->ClearPref(prefs::kHideFullscreenToolbar);
  }
#endif

  // Added 12/1015.
  profile_prefs->ClearPref(kURLsToRestoreOnStartupOld);
  profile_prefs->ClearPref(kRestoreStartupURLsMigrationTime);

  // Added 12/2015.
  profile_prefs->ClearPref(kRestoreOnStartupMigrated);

#if defined(USE_AURA)
  // Added 1/2016
  profile_prefs->ClearPref(kFlingMaxCancelToDownTimeInMs);
  profile_prefs->ClearPref(kFlingMaxTapGapTimeInMs);
  profile_prefs->ClearPref(kTabScrubActivationDelayInMs);
  profile_prefs->ClearPref(kMaxSeparationForGestureTouchesInPixels);
  profile_prefs->ClearPref(kSemiLongPressTimeInMs);
  profile_prefs->ClearPref(kOverscrollHorizontalThresholdComplete);
  profile_prefs->ClearPref(kOverscrollVerticalThresholdComplete);
  profile_prefs->ClearPref(kOverscrollMinimumThresholdStart);
  profile_prefs->ClearPref(kOverscrollMinimumThresholdStartTouchpad);
  profile_prefs->ClearPref(kOverscrollVerticalThresholdStart);
  profile_prefs->ClearPref(kOverscrollHorizontalResistThreshold);
  profile_prefs->ClearPref(kOverscrollVerticalResistThreshold);
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_GOOGLE_NOW)
  // Added 3/2016.
  profile_prefs->ClearPref(kGoogleGeolocationAccessEnabled);
#endif

  // Added 4/2016.
  if (!profile_prefs->GetBoolean(kCheckDefaultBrowser)) {
    // Seed kDefaultBrowserLastDeclined with the install date.
    metrics::MetricsService* metrics_service =
        g_browser_process->metrics_service();
    base::Time install_time =
        metrics_service
            ? base::Time::FromTimeT(metrics_service->GetInstallDate())
            : base::Time::Now();
    profile_prefs->SetInt64(prefs::kDefaultBrowserLastDeclined,
                            install_time.ToInternalValue());
  }
  profile_prefs->ClearPref(kCheckDefaultBrowser);
}

}  // namespace chrome
