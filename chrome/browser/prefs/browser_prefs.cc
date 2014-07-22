// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <string>

#include "base/debug/trace_event.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/apps/drive/drive_app_mapping.h"
#include "chrome/browser/apps/shortcut_manager.h"
#include "chrome/browser/autocomplete/zero_suggest_provider.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/recovery_component_installer.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/geolocation/geolocation_prefs.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/gpu/gl_string_manager.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/net/http_server_properties_manager_factory.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/signin/easy_unlock.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/startup/autolaunch_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/network_time/network_time_tracker.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/rappor/rappor_service.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "net/http/http_server_properties_manager.h"

#if defined(ENABLE_AUTOFILL_DIALOG)
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "apps/prefs.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/launch_util.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/new_tab_page_prefs.h"
#else
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_mode_detector.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/invalidation/invalidator_storage.h"
#else
#include "chrome/browser/extensions/default_apps.h"
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

namespace {

enum MigratedPreferences {
  NO_PREFS = 0,
  DNS_PREFS = 1 << 0,
  WINDOWS_PREFS = 1 << 1,
  GOOGLE_URL_TRACKER_PREFS = 1 << 2,
};

// A previous feature (see
// chrome/browser/protector/protected_prefs_watcher.cc in source
// control history) used this string as a prefix for various prefs it
// registered. We keep it here for now to clear out those old prefs in
// MigrateUserPrefs.
const char kBackupPref[] = "backup";

#if !defined(OS_ANDROID)
// The sync promo error message preference has been removed; this pref will
// be cleared from user data.
const char kSyncPromoErrorMessage[] = "sync_promo.error_message";
#endif

}  // namespace

namespace chrome {

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Prefs in Local State.
  registry->RegisterIntegerPref(prefs::kMultipleProfilePrefMigration, 0);

  // Please keep this list alphabetized.
  AppListService::RegisterPrefs(registry);
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  chrome_prefs::RegisterPrefs(registry);
  chrome_variations::VariationsService::RegisterPrefs(registry);
  component_updater::RegisterPrefsForRecoveryComponent(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  FlagsUI::RegisterPrefs(registry);
  geolocation::RegisterPrefs(registry);
  GLStringManager::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  IOThread::RegisterPrefs(registry);
  KeywordEditorController::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  profiles::RegisterPrefs(registry);
  PromoResourceService::RegisterPrefs(registry);
  rappor::RapporService::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  SigninManagerFactory::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  WebCacheManager::RegisterPrefs(registry);

#if defined(ENABLE_AUTOFILL_DIALOG)
  autofill::AutofillDialogController::RegisterPrefs(registry);
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
#endif

#if defined(ENABLE_NOTIFICATIONS) && !defined(OS_ANDROID)
  // Android does not use the message center for notifications.
  MessageCenterNotificationManager::RegisterPrefs(registry);
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

#if !defined(OS_ANDROID)
  AutomaticProfileResetterFactory::RegisterPrefs(registry);
  BackgroundModeManager::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
#if !defined(OS_CHROMEOS)
  RegisterDefaultBrowserPromptPrefs(registry);
#endif  // !defined(OS_CHROMEOS)
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  chromeos::ChargerReplacementHandler::RegisterPrefs(registry);
  chromeos::DataPromoNotification::RegisterPrefs(registry);
  chromeos::DeviceOAuth2TokenService::RegisterPrefs(registry);
  chromeos::device_settings_cache::RegisterPrefs(registry);
  chromeos::default_pinned_apps_field_trial::RegisterPrefs(registry);
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
  chromeos::UserImageManager::RegisterPrefs(registry);
  chromeos::UserManager::RegisterPrefs(registry);
  chromeos::UserSessionManager::RegisterPrefs(registry);
  chromeos::WallpaperManager::RegisterPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  extensions::ExtensionAssetsManagerChromeOS::RegisterPrefs(registry);
  invalidation::InvalidatorStorage::RegisterPrefs(registry);
  policy::AutoEnrollmentClient::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorChromeOS::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
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
  TRACE_EVENT0("browser", "chrome::RegisterUserPrefs");
  // User prefs. Please keep this list alphabetized.
  autofill::AutofillManager::RegisterProfilePrefs(registry);
  bookmarks::RegisterProfilePrefs(registry);
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
  easy_unlock::RegisterProfilePrefs(registry);
  gcm::GCMProfileService::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  InstantUI::RegisterProfilePrefs(registry);
  NavigationCorrectionTabObserver::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaStreamDevicesController::RegisterProfilePrefs(registry);
  NetPrefObserver::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  PromoResourceService::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

#if defined(ENABLE_AUTOFILL_DIALOG)
  autofill::AutofillDialogController::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::URLBlacklistManager::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_EXTENSIONS)
  apps::RegisterProfilePrefs(registry);
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::launch_util::RegisterProfilePrefs(registry);
#endif
  // TODO(thestig) These should be in ifdef'd out, but too many parts of Chrome
  // still expects it to be registered.
  ExtensionWebUI::RegisterProfilePrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);

#if defined(ENABLE_FULL_PRINTING)
  print_dialog_cloud::RegisterProfilePrefs(registry);
  printing::StickySettings::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_MANAGED_USERS)
  SupervisedUserService::RegisterProfilePrefs(registry);
  SupervisedUserSharedSettingsService::RegisterProfilePrefs(registry);
  SupervisedUserSyncService::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationService::RegisterProfilePrefs(registry);
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
  LocalDiscoveryUI::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  chrome_variations::VariationsService::RegisterProfilePrefs(registry);
  NewTabPagePrefs::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
#else
  AppShortcutManager::RegisterProfilePrefs(registry);
  autofill::GeneratedCreditCardBubbleController::RegisterUserPrefs(registry);
  DeviceIDFetcher::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  DriveAppMapping::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::ExtensionSettingsHandler::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  PepperFlashSettingsManager::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  PluginsUI::RegisterProfilePrefs(registry);
  RegisterAutolaunchUserPrefs(registry);
  signin::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  chromeos::attestation::PlatformVerificationFlow::RegisterProfilePrefs(
      registry);
  chromeos::first_run::RegisterProfilePrefs(registry);
  chromeos::file_system_provider::RegisterProfilePrefs(registry);
  chromeos::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  chromeos::proxy_config::RegisterProfilePrefs(registry);
  chromeos::SAMLOfflineSigninLimiter::RegisterProfilePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::UserImageSyncObserver::RegisterProfilePrefs(registry);
  extensions::EnterprisePlatformKeysPrivateChallengeUserKeyFunction::
      RegisterProfilePrefs(registry);
  FlagsUI::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN)
  NetworkProfileBubble::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewProfilePrefs(registry);
  RegisterInvertBubbleUserPrefs(registry);
#endif

#if defined(USE_ASH)
  ash::RegisterChromeLauncherUserPrefs(registry);
#endif

  // Prefs registered only for migration (clearing or moving to a new
  // key) go here.
  registry->RegisterDictionaryPref(
      kBackupPref,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if !defined(OS_ANDROID)
  registry->RegisterStringPref(
      kSyncPromoErrorMessage,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
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

void MigrateUserPrefs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();

  // Cleanup prefs from now-removed protector feature.
  prefs->ClearPref(kBackupPref);

#if !defined(OS_ANDROID)
  // Cleanup now-removed sync promo error message preference.
  // TODO(fdoray): Remove this when it's safe to do so (crbug.com/268442).
  prefs->ClearPref(kSyncPromoErrorMessage);
#endif

  PromoResourceService::MigrateUserPrefs(prefs);
  translate::TranslatePrefs::MigrateUserPrefs(prefs, prefs::kAcceptLanguages);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  autofill::AutofillManager::MigrateUserPrefs(prefs);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

void MigrateBrowserPrefs(Profile* profile, PrefService* local_state) {
  // Copy pref values which have been migrated to user_prefs from local_state,
  // or remove them from local_state outright, if copying is not required.
  int current_version =
      local_state->GetInteger(prefs::kMultipleProfilePrefMigration);
  PrefRegistrySimple* registry = static_cast<PrefRegistrySimple*>(
      local_state->DeprecatedGetPrefRegistry());

  if (!(current_version & DNS_PREFS)) {
    registry->RegisterListPref(prefs::kDnsStartupPrefetchList);
    local_state->ClearPref(prefs::kDnsStartupPrefetchList);

    registry->RegisterListPref(prefs::kDnsHostReferralList);
    local_state->ClearPref(prefs::kDnsHostReferralList);

    current_version |= DNS_PREFS;
    local_state->SetInteger(prefs::kMultipleProfilePrefMigration,
                            current_version);
  }

  PrefService* user_prefs = profile->GetPrefs();
  if (!(current_version & WINDOWS_PREFS)) {
    registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
    if (local_state->HasPrefPath(prefs::kBrowserWindowPlacement)) {
      const PrefService::Preference* pref =
          local_state->FindPreference(prefs::kBrowserWindowPlacement);
      DCHECK(pref);
      user_prefs->Set(prefs::kBrowserWindowPlacement,
                      *(pref->GetValue()));
    }
    local_state->ClearPref(prefs::kBrowserWindowPlacement);

    current_version |= WINDOWS_PREFS;
    local_state->SetInteger(prefs::kMultipleProfilePrefMigration,
                            current_version);
  }

  if (!(current_version & GOOGLE_URL_TRACKER_PREFS)) {
    registry->RegisterStringPref(prefs::kLastKnownGoogleURL,
                                 GoogleURLTracker::kDefaultGoogleHomepage);
    if (local_state->HasPrefPath(prefs::kLastKnownGoogleURL)) {
      user_prefs->SetString(prefs::kLastKnownGoogleURL,
                            local_state->GetString(prefs::kLastKnownGoogleURL));
    }
    local_state->ClearPref(prefs::kLastKnownGoogleURL);

    registry->RegisterStringPref(prefs::kLastPromptedGoogleURL,
                                 std::string());
    if (local_state->HasPrefPath(prefs::kLastPromptedGoogleURL)) {
      user_prefs->SetString(
          prefs::kLastPromptedGoogleURL,
          local_state->GetString(prefs::kLastPromptedGoogleURL));
    }
    local_state->ClearPref(prefs::kLastPromptedGoogleURL);

    current_version |= GOOGLE_URL_TRACKER_PREFS;
    local_state->SetInteger(prefs::kMultipleProfilePrefMigration,
                            current_version);
  }

#if defined(OS_CHROMEOS)
  chromeos::default_pinned_apps_field_trial::MigratePrefs(local_state);
#endif

#if defined(TOOLKIT_VIEWS)
  MigrateBrowserTabStripPrefs(local_state);
#endif
}

}  // namespace chrome
