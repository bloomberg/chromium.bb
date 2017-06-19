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
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/geolocation/geolocation_prefs.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/gpu/gpu_profile_cache.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/net/http_server_properties_manager_factory.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/notifications/extension_welcome_notification.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/origin_trial_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/net_http_session_params_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
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
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/ct_policy_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/doodle/doodle_service.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_snippets/breaking_news/content_suggestions_gcm_app_handler.h"
#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/request_throttler.h"
#include "components/ntp_snippets/sessions/foreign_sessions_suggestions_provider.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/offline_pages/features/features.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/translate/core/browser/language_model.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/features/features.h"
#include "net/http/http_server_properties_manager.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"
#include "rlz/features/features.h"

#if BUILDFLAG(ENABLE_APP_LIST)
#include "chrome/browser/apps/drive/drive_app_mapping.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/shortcut_manager.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/android/offline_pages/prefetch/prefetch_background_task.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/ntp/content_suggestions_notifier_service.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"
#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#else
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/upgrade_detector.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/file_system_provider/registry.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/onc/onc_pref_names.h"
#include "components/quirks/quirks_manager.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/apps/app_launch_for_metro_restart_win.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "chrome/browser/ui/webui/md_history_ui.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
#endif

namespace {

// Deprecated 8/2016.
constexpr char kRecentlySelectedEncoding[] =
    "profile.recently_selected_encodings";
constexpr char kStaticEncodings[] = "intl.static_encodings";

// Deprecated 9/2016.
constexpr char kWebKitUsesUniversalDetector[] =
    "webkit.webprefs.uses_universal_detector";
constexpr char kWebKitAllowDisplayingInsecureContent[] =
    "webkit.webprefs.allow_displaying_insecure_content";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Deprecated 2/2017.
constexpr char kToolbarMigratedComponentActionStatus[] =
    "toolbar_migrated_component_action_status";
#endif

#if BUILDFLAG(ENABLE_RLZ)
// Migrated out of kDistroDict as of 2/2017.
constexpr char kDistroRlzPingDelay[] = "ping_delay";
#endif  // BUILDFLAG(ENABLE_RLZ)

// master_preferences used to be mapped as-is to Preferences on first run but
// the "distribution" dictionary was never used beyond first run. It is now
// stripped in first_run.cc prior to applying this mapping. Cleanup for existing
// Preferences files added here 2/2017.
constexpr char kDistroDict[] = "distribution";

}  // namespace

namespace chrome {

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Please keep this list alphabetized.
  AppListService::RegisterPrefs(registry);
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  ChromeTracingDelegate::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
  component_updater::RegisterPrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  geolocation::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  GpuProfileCache::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  IOThread::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  OriginTrialPrefs::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  profiles::RegisterPrefs(registry);
  rappor::RapporServiceImpl::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  SigninManagerFactory::RegisterPrefs(registry);
  ssl_config::SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(registry);
  startup_metric_utils::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);

  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EasyUnlockService::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginFinder::RegisterPrefs(registry);
  PluginsResourceService::RegisterPrefs(registry);
#endif

#if defined(OS_ANDROID)
  ::android::RegisterPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_BACKGROUND)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  RegisterBrowserPrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  // The native GCM is used on Android instead.
  gcm::GCMChannelStatusSyncer::RegisterPrefs(registry);
  gcm::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
#if !defined(OS_CHROMEOS)
  RegisterDefaultBrowserPromptPrefs(registry);
#endif  // !defined(OS_CHROMEOS)
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::ArcKioskAppManager::RegisterPrefs(registry);
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
  chromeos::NetworkThrottlingObserver::RegisterPrefs(registry);
  chromeos::Preferences::RegisterPrefs(registry);
  chromeos::RegisterDisplayLocalStatePrefs(registry);
  chromeos::ResetScreenHandler::RegisterPrefs(registry);
  chromeos::ResourceReporter::RegisterPrefs(registry);
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
  ::onc::RegisterPrefs(registry);
  policy::AutoEnrollmentClient::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorChromeOS::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DMTokenStorage::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);

  // Moved to profile prefs, but we still need to register the prefs in local
  // state until migration is complete (See MigrateObsoleteBrowserPrefs()).
  chromeos::system::InputDeviceSettings::RegisterProfilePrefs(registry);
#endif

#if defined(OS_MACOSX)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
#endif

#if defined(OS_WIN)
  app_metro_launch::RegisterPrefs(registry);
  component_updater::RegisterPrefsForSwReporter(registry);
  desktop_ios_promotion::RegisterLocalPrefs(registry);
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
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::HttpServerPropertiesManagerFactory::RegisterProfilePrefs(
      registry);
  chrome_browser_net::Predictor::RegisterProfilePrefs(registry);
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  doodle::DoodleService::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  InstantUI::RegisterProfilePrefs(registry);
  NavigationCorrectionTabObserver::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaStreamDevicesController::RegisterProfilePrefs(registry);
  NetHttpSessionParamsObserver::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_snippets::ContentSuggestionsGCMAppHandler::RegisterProfilePrefs(registry);
  ntp_snippets::ContentSuggestionsService::RegisterProfilePrefs(registry);
  ntp_snippets::ForeignSessionsSuggestionsProvider::RegisterProfilePrefs(
      registry);
  ntp_snippets::RemoteSuggestionsProviderImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RequestThrottler::RegisterProfilePrefs(registry);
  ntp_snippets::SubscriptionManager::RegisterProfilePrefs(registry);
  ntp_snippets::UserClassifier::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  password_bubble_experiment::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::LanguageModel::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  UINetworkQualityEstimatorService::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);

  policy::URLBlacklistManager::RegisterProfilePrefs(registry);
  certificate_transparency::CTPolicyManager::RegisterPrefs(registry);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::RegisterProfilePrefs(registry);
  RegisterAnimationPolicyPrefs(registry);
  ToolbarActionsBar::RegisterProfilePrefs(registry);
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::launch_util::RegisterProfilePrefs(registry);
  extensions::NtpOverriddenBubbleDelegate::RegisterPrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_ANDROID)
  // The extension welcome notification requires a build that enables extensions
  // and notifications, and uses the UI message center.
  ExtensionWelcomeNotification::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::StickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  LocalDiscoveryUI::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#if !defined(OS_ANDROID)
  SupervisedUserSharedSettingsService::RegisterProfilePrefs(registry);
  SupervisedUserSyncService::RegisterProfilePrefs(registry);
#endif
  ChildAccountService::RegisterProfilePrefs(registry);
  SupervisedUserService::RegisterProfilePrefs(registry);
  SupervisedUserWhitelistService::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  GeolocationPermissionContextAndroid::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
#else
  AppShortcutManager::RegisterProfilePrefs(registry);
  DeviceIDFetcher::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENABLE_APP_LIST)
  DriveAppMapping::RegisterProfilePrefs(registry);
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
#endif
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::ExtensionSettingsHandler::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  PepperFlashSettingsManager::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  ContentSuggestionsNotifierService::RegisterProfilePrefs(registry);
  DownloadSuggestionsProvider::RegisterProfilePrefs(registry);
  ntp_snippets::ClickBasedCategoryRanker::RegisterProfilePrefs(registry);
  ntp_snippets::PhysicalWebPageSuggestionsProvider::RegisterProfilePrefs(
      registry);
  ntp_snippets::RecentTabSuggestionsProvider::RegisterProfilePrefs(registry);
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  arc::ArcSessionManager::RegisterProfilePrefs(registry);
  arc::ArcPolicyBridge::RegisterProfilePrefs(registry);
  chromeos::first_run::RegisterProfilePrefs(registry);
  chromeos::file_system_provider::RegisterProfilePrefs(registry);
  chromeos::KeyPermissions::RegisterProfilePrefs(registry);
  chromeos::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::PinStorage::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  chromeos::PrintersManager::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::RegisterProfilePrefs(registry);
  chromeos::SAMLOfflineSigninLimiter::RegisterProfilePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::system::InputDeviceSettings::RegisterProfilePrefs(registry);
  chromeos::UserImageSyncObserver::RegisterProfilePrefs(registry);
  extensions::EPKPChallengeUserKey::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  policy::DeviceStatusCollector::RegisterProfilePrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_APP_LIST)
  ArcAppListPrefs::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN)
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  desktop_ios_promotion::RegisterProfilePrefs(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
  safe_browsing::SettingsResetPromptPrefsManager::RegisterProfilePrefs(
      registry);
  safe_browsing::PostCleanupSettingsResetter::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewProfilePrefs(registry);
  RegisterInvertBubbleUserPrefs(registry);
#endif

#if defined(USE_ASH)
  RegisterChromeLauncherUserPrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  MdHistoryUI::RegisterProfilePrefs(registry);
  settings::MdSettingsUI::RegisterProfilePrefs(registry);
#endif

  // Preferences registered only for migration (clearing or moving to a new key)
  // go here.

  registry->RegisterStringPref(kStaticEncodings, std::string());
  registry->RegisterStringPref(kRecentlySelectedEncoding, std::string());
  registry->RegisterBooleanPref(kWebKitUsesUniversalDetector, true);

  registry->RegisterBooleanPref(kWebKitAllowDisplayingInsecureContent, true);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterDictionaryPref(kToolbarMigratedComponentActionStatus);
#endif

  registry->RegisterDictionaryPref(kDistroDict);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::RegisterPrefetchBackgroundTaskPrefs(registry);
#endif
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);

#if defined(OS_CHROMEOS)
  chromeos::PowerPrefs::RegisterUserProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  ::android::RegisterUserProfilePrefs(registry);
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
#if defined(OS_CHROMEOS)
  // Added 11/2016
  local_state->ClearPref(prefs::kTouchscreenEnabled);
  local_state->ClearPref(prefs::kTouchpadEnabled);
#endif  // defined(OS_CHROMEOS)
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  PrefService* profile_prefs = profile->GetPrefs();

  // Added 8/2016.
  profile_prefs->ClearPref(kStaticEncodings);
  profile_prefs->ClearPref(kRecentlySelectedEncoding);

  // Added 9/2016.
  profile_prefs->ClearPref(kWebKitUsesUniversalDetector);
  profile_prefs->ClearPref(kWebKitAllowDisplayingInsecureContent);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Added 2/2017.
  // NOTE(takumif): When removing this code, also remove the following tests:
  //  - MediaRouterUIBrowserTest.MigrateToolbarIconShownPref
  //  - MediaRouterUIBrowserTest.MigrateToolbarIconUnshownPref
  {
    bool show_cast_icon = false;
    const base::DictionaryValue* action_migration_dict =
        profile_prefs->GetDictionary(kToolbarMigratedComponentActionStatus);
    if (action_migration_dict &&
        action_migration_dict->GetBoolean(
            ComponentToolbarActionsFactory::kMediaRouterActionId,
            &show_cast_icon)) {
      profile_prefs->SetBoolean(prefs::kShowCastIconInToolbar, show_cast_icon);
    }
    profile_prefs->ClearPref(kToolbarMigratedComponentActionStatus);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Added 2/2017.
  {
#if BUILDFLAG(ENABLE_RLZ)
    const base::DictionaryValue* distro_dict =
        profile_prefs->GetDictionary(kDistroDict);
    int rlz_ping_delay = 0;
    if (distro_dict &&
        distro_dict->GetInteger(kDistroRlzPingDelay, &rlz_ping_delay)) {
      profile_prefs->SetInteger(prefs::kRlzPingDelaySeconds, rlz_ping_delay);
    }
#endif  // BUILDFLAG(ENABLE_RLZ)
    profile_prefs->ClearPref(kDistroDict);
  }
}

std::set<PrefValueStore::PrefStoreType> ExpectedPrefStores() {
  return std::set<PrefValueStore::PrefStoreType>({
      PrefValueStore::MANAGED_STORE,
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
      PrefValueStore::SUPERVISED_USER_STORE,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
      PrefValueStore::EXTENSION_STORE,
#endif
      PrefValueStore::COMMAND_LINE_STORE, PrefValueStore::RECOMMENDED_STORE,
      PrefValueStore::USER_STORE, PrefValueStore::DEFAULT_STORE
  });
}

std::set<PrefValueStore::PrefStoreType> InProcessPrefStores() {
  auto pref_stores = ExpectedPrefStores();
  // Until we have a distinction between owned and unowned prefs, we always have
  // default values for all prefs locally. Since we already have the defaults it
  // would be wasteful to request them from the service by connecting to the
  // DEFAULT_STORE.
  // TODO(sammc): Once we have this distinction, connect to the default pref
  // store here (by erasing it from |pref_stores|).
  pref_stores.erase(PrefValueStore::USER_STORE);
  return pref_stores;
}

}  // namespace chrome
