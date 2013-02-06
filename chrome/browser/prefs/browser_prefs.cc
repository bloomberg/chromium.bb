// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_prompt_prefs.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/app_launcher.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/geolocation/geolocation_prefs.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/net/http_server_properties_manager.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_prefs_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_registry_simple.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/startup/autolaunch_prompt.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_process_host.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/browser/policy/url_blacklist_manager.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_layout_type_prefs.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/policy/auto_enrollment_client.h"
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/device_status_collector.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/chrome_to_mobile_service.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/ntp/android/promo_handler.h"
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/web_resource/plugins_resource_service.h"
#endif

namespace {

enum MigratedPreferences {
  NO_PREFS = 0,
  DNS_PREFS = 1 << 0,
  WINDOWS_PREFS = 1 << 1,
  GOOGLE_URL_TRACKER_PREFS = 1 << 2,
};

}  // namespace

namespace chrome {

// TODO(joi): Do the work needed to remove the PrefService parameter,
// i.e. to do all registration up front before a PrefService is even
// created.
void RegisterLocalState(PrefRegistrySimple* registry,
                        PrefService* local_state) {
  // Prefs in Local State.
  registry->RegisterIntegerPref(prefs::kMultipleProfilePrefMigration, 0);

  // Please keep this list alphabetized.
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  chrome::RegisterScreenshotPrefs(registry);
  extensions::app_launcher::RegisterPrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  FlagsUI::RegisterPrefs(registry);
  geolocation::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  KeywordEditorController::RegisterPrefs(registry);
  MetricsLog::RegisterPrefs(registry);
  MetricsService::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  ProfileManager::RegisterPrefs(registry);
  PromoResourceService::RegisterPrefs(registry);
  SigninManagerFactory::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  WebCacheManager::RegisterPrefs(registry);

#if defined(ENABLE_PLUGINS)
  PluginFinder::RegisterPrefs(registry);
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginsResourceService::RegisterPrefs(registry);
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
#endif

#if defined(ENABLE_NOTIFICATIONS)
  NotificationPrefsManager::RegisterPrefs(local_state, registry);
#endif

#if defined(ENABLE_TASK_MANAGER)
  TaskManager::RegisterPrefs(registry);
#endif  // defined(ENABLE_TASK_MANAGER)

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewPrefs(registry);
  RegisterTabStripLayoutTypePrefs(registry);
#endif

#if !defined(OS_ANDROID)
  BackgroundModeManager::RegisterPrefs(registry);
  chrome_variations::VariationsService::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
  ManagedMode::RegisterPrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  chromeos::AudioHandler::RegisterPrefs(registry);
  chromeos::DataPromoNotification::RegisterPrefs(registry);
  chromeos::device_settings_cache::RegisterPrefs(registry);
  chromeos::language_prefs::RegisterPrefs(registry);
  chromeos::ProxyConfigServiceImpl::RegisterPrefs(registry);
  chromeos::RegisterDisplayLocalStatePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterPrefs(registry);
  chromeos::UserImageManager::RegisterPrefs(registry);
  chromeos::UserManager::RegisterPrefs(registry);
  chromeos::WallpaperManager::RegisterPrefs(registry);
  chromeos::WizardController::RegisterPrefs(registry);
  policy::AutoEnrollmentClient::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
#endif

#if defined(OS_MACOSX)
  confirm_quit::RegisterLocalState(registry);
#endif

#if defined(ENABLE_SETTINGS_APP)
  chrome::RegisterAppListPrefs(registry);
#endif
}

void RegisterUserPrefs(PrefServiceSyncable* user_prefs) {
  // User prefs. Please keep this list alphabetized.
  AlternateErrorPageTabObserver::RegisterUserPrefs(user_prefs);
  AutofillManager::RegisterUserPrefs(user_prefs);
  BookmarkPromptPrefs::RegisterUserPrefs(user_prefs);
  bookmark_utils::RegisterUserPrefs(user_prefs);
  BrowserInstantController::RegisterUserPrefs(user_prefs);
  browser_sync::SyncPrefs::RegisterUserPrefs(user_prefs);
  ChromeContentBrowserClient::RegisterUserPrefs(user_prefs);
  ChromeVersionService::RegisterUserPrefs(user_prefs);
  chrome_browser_net::HttpServerPropertiesManager::RegisterUserPrefs(
      user_prefs);
  chrome_browser_net::Predictor::RegisterUserPrefs(user_prefs);
  DownloadPrefs::RegisterUserPrefs(user_prefs);
  extensions::ComponentLoader::RegisterUserPrefs(user_prefs);
  extensions::ExtensionPrefs::RegisterUserPrefs(user_prefs);
  ExtensionWebUI::RegisterUserPrefs(user_prefs);
  first_run::RegisterUserPrefs(user_prefs);
  HostContentSettingsMap::RegisterUserPrefs(user_prefs);
  IncognitoModePrefs::RegisterUserPrefs(user_prefs);
  InstantUI::RegisterUserPrefs(user_prefs);
  MediaCaptureDevicesDispatcher::RegisterUserPrefs(user_prefs);
  MediaStreamDevicesController::RegisterUserPrefs(user_prefs);
  NetPrefObserver::RegisterUserPrefs(user_prefs);
  NewTabUI::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  PrefProxyConfigTrackerImpl::RegisterUserPrefs(user_prefs);
  PrefsTabHelper::RegisterUserPrefs(user_prefs);
  ProfileImpl::RegisterUserPrefs(user_prefs);
  PromoResourceService::RegisterUserPrefs(user_prefs);
  ProtocolHandlerRegistry::RegisterUserPrefs(user_prefs);
  RegisterBrowserUserPrefs(user_prefs);
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  TranslatePrefs::RegisterUserPrefs(user_prefs);

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::URLBlacklistManager::RegisterUserPrefs(user_prefs);
#endif

#if defined(ENABLE_MANAGED_USERS)
  ManagedUserService::RegisterUserPrefs(user_prefs);
#endif

#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationService::RegisterUserPrefs(user_prefs);
#endif

#if defined(ENABLE_WEB_INTENTS)
  web_intents::RegisterUserPrefs(user_prefs);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterInvertBubbleUserPrefs(user_prefs);
#elif defined(TOOLKIT_GTK)
  BrowserWindowGtk::RegisterUserPrefs(user_prefs);
#endif

#if defined(OS_ANDROID)
  PromoHandler::RegisterUserPrefs(user_prefs);
#endif

#if defined(USE_ASH)
  ash::RegisterChromeLauncherUserPrefs(user_prefs);
#endif

#if !defined(OS_ANDROID)
  TabsCaptureVisibleTabFunction::RegisterUserPrefs(user_prefs);
  ChromeToMobileService::RegisterUserPrefs(user_prefs);
  DevToolsWindow::RegisterUserPrefs(user_prefs);
  extensions::CommandService::RegisterUserPrefs(user_prefs);
  ExtensionSettingsHandler::RegisterUserPrefs(user_prefs);
  PepperFlashSettingsManager::RegisterUserPrefs(user_prefs);
  PinnedTabCodec::RegisterUserPrefs(user_prefs);
  PluginsUI::RegisterUserPrefs(user_prefs);
  printing::StickySettings::RegisterUserPrefs(user_prefs);
  RegisterAutolaunchUserPrefs(user_prefs);
  SyncPromoUI::RegisterUserPrefs(user_prefs);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterUserPrefs(user_prefs);
#endif

#if defined(OS_CHROMEOS)
  chromeos::Preferences::RegisterUserPrefs(user_prefs);
  chromeos::ProxyConfigServiceImpl::RegisterUserPrefs(user_prefs);
#endif

#if defined(OS_WIN)
  NetworkProfileBubble::RegisterUserPrefs(user_prefs);
#endif
}

void MigrateUserPrefs(Profile* profile) {
  // Cleanup old prefs.
  static const char kBackupPref[] = "backup";
  PrefServiceSyncable* prefs = profile->GetPrefs();
  prefs->RegisterDictionaryPref(kBackupPref, new DictionaryValue(),
                                PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->ClearPref(kBackupPref);
  prefs->UnregisterPreference(kBackupPref);
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

  PrefServiceSyncable* user_prefs = profile->GetPrefs();
  if (!(current_version & WINDOWS_PREFS)) {
    registry->RegisterIntegerPref(prefs::kDevToolsHSplitLocation, -1);
    if (local_state->HasPrefPath(prefs::kDevToolsHSplitLocation)) {
      user_prefs->SetInteger(
          prefs::kDevToolsHSplitLocation,
          local_state->GetInteger(prefs::kDevToolsHSplitLocation));
    }
    local_state->ClearPref(prefs::kDevToolsHSplitLocation);

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
    GoogleURLTrackerFactory::GetInstance()->RegisterUserPrefsOnProfile(profile);
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
}

}  // namespace chrome
