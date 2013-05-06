// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "apps/app_restore_service_factory.h"
#include "apps/shortcut_manager_factory.h"
#include "chrome/browser/autofill/autocheckout_whitelist_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/activity_log.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/audio/audio_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/idle/idle_manager_factory.h"
#include "chrome/browser/extensions/api/input/input.h"
#include "chrome/browser/extensions/api/managed_mode_private/managed_mode_private_api.h"
#include "chrome/browser/extensions/api/management/management_api.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/api/session_restore/session_restore_api.h"
#include "chrome/browser/extensions/api/spellcheck/spellcheck_api.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/api/system_info/system_info_api.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/shortcuts_backend_factory.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/user_style_sheet_watcher_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/networking_private_event_router_factory.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#if defined(FILE_MANAGER_EXTENSION)
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"
#endif
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#endif

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/common/chrome_switches.h"
#endif

class Profile;

void ProfileDependencyManager::AddComponent(
    ProfileKeyedBaseFactory* component) {
  dependency_graph_.AddNode(component);
}

void ProfileDependencyManager::RemoveComponent(
    ProfileKeyedBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void ProfileDependencyManager::AddEdge(ProfileKeyedBaseFactory* depended,
                                       ProfileKeyedBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void ProfileDependencyManager::CreateProfileServices(Profile* profile,
                                                     bool is_testing_profile) {
#ifndef NDEBUG
  // Unmark |profile| as dead. This exists because of unit tests, which will
  // often have similar stack structures. 0xWhatever might be created, go out
  // of scope, and then a new Profile object might be created at 0xWhatever.
  dead_profile_pointers_.erase(profile);
#endif

  AssertFactoriesBuilt();

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpProfileDependencies(profile);
#endif

  for (size_t i = 0; i < construction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(construction_order[i]);

    if (!profile->IsOffTheRecord()) {
      // We only register preferences on normal profiles because the incognito
      // profile shares the pref service with the normal one.
      factory->RegisterUserPrefsOnProfile(profile);
    }

    if (is_testing_profile && factory->ServiceIsNULLWhileTesting()) {
      factory->SetEmptyTestingFactory(profile);
    } else if (factory->ServiceIsCreatedWithProfile()) {
      // Create the service.
      factory->CreateServiceNow(profile);
    }
  }
}

void ProfileDependencyManager::DestroyProfileServices(Profile* profile) {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpProfileDependencies(profile);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(destruction_order[i]);
    factory->ProfileShutdown(profile);
  }

#ifndef NDEBUG
  // The profile is now dead to the rest of the program.
  dead_profile_pointers_.insert(profile);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(destruction_order[i]);
    factory->ProfileDestroyed(profile);
  }
}

#ifndef NDEBUG
void ProfileDependencyManager::AssertProfileWasntDestroyed(Profile* profile) {
  if (dead_profile_pointers_.find(profile) != dead_profile_pointers_.end()) {
    NOTREACHED() << "Attempted to access a Profile that was ShutDown(). This "
                 << "is most likely a heap smasher in progress. After "
                 << "ProfileKeyedService::Shutdown() completes, your service "
                 << "MUST NOT refer to depended Profile services again.";
  }
}
#endif

// static
ProfileDependencyManager* ProfileDependencyManager::GetInstance() {
  return Singleton<ProfileDependencyManager>::get();
}

ProfileDependencyManager::ProfileDependencyManager() : built_factories_(false) {
}

ProfileDependencyManager::~ProfileDependencyManager() {}

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes itself and registers its dependencies with
// the global PreferenceDependencyManager. We need to have a complete
// dependency graph when we create a profile so we can dispatch the profile
// creation message to the services that want to create their services at
// profile creation time.
//
// TODO(erg): This needs to be something else. I don't think putting every
// FooServiceFactory here will scale or is desirable long term.
void ProfileDependencyManager::AssertFactoriesBuilt() {
  if (built_factories_)
    return;

  AboutSigninInternalsFactory::GetInstance();

#if defined(ENABLE_BACKGROUND)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalServiceFactory::GetInstance();
#endif
  ChromeGeolocationPermissionContextFactory::GetInstance();
#if defined(ENABLE_PRINTING)
  CloudPrintProxyServiceFactory::GetInstance();
#endif
  CookieSettings::Factory::GetInstance();
#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationServiceFactory::GetInstance();
#endif
  DownloadServiceFactory::GetInstance();
#if defined(ENABLE_EXTENSIONS)
  apps::AppRestoreServiceFactory::GetInstance();
  apps::ShortcutManagerFactory::GetInstance();
  autofill::autocheckout::WhitelistManagerFactory::GetInstance();
  extensions::ActivityLogFactory::GetInstance();
  extensions::AlarmManager::GetFactoryInstance();
  extensions::AudioAPI::GetFactoryInstance();
  extensions::BookmarksAPI::GetFactoryInstance();
  extensions::BluetoothAPIFactory::GetInstance();
  extensions::CommandService::GetFactoryInstance();
  extensions::CookiesAPI::GetFactoryInstance();
  extensions::DialAPIFactory::GetInstance();
  extensions::ExtensionActionAPI::GetFactoryInstance();
  extensions::ExtensionSystemFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::FontSettingsAPI::GetFactoryInstance();
  extensions::HistoryAPI::GetFactoryInstance();
  extensions::IdentityAPI::GetFactoryInstance();
  extensions::IdleManagerFactory::GetInstance();
  extensions::InstallTrackerFactory::GetInstance();
#if defined(TOOLKIT_VIEWS)
  extensions::InputAPI::GetFactoryInstance();
#endif
#if defined(OS_CHROMEOS)
  extensions::InputImeAPI::GetFactoryInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
#endif
  extensions::ManagedModeAPI::GetFactoryInstance();
  extensions::ManagementAPI::GetFactoryInstance();
  extensions::MediaGalleriesPrivateAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  extensions::MediaPlayerAPI::GetFactoryInstance();
#endif
  extensions::OmniboxAPI::GetFactoryInstance();
  extensions::PreferenceAPI::GetFactoryInstance();
  extensions::ProcessesAPI::GetFactoryInstance();
  extensions::PushMessagingAPI::GetFactoryInstance();
  extensions::SessionRestoreAPI::GetFactoryInstance();
  extensions::SpellcheckAPI::GetFactoryInstance();
  extensions::StreamsPrivateAPI::GetFactoryInstance();
  extensions::SystemInfoAPI::GetFactoryInstance();
  extensions::SuggestedLinksRegistryFactory::GetInstance();
  extensions::TabCaptureRegistryFactory::GetInstance();
  extensions::TabsWindowsAPI::GetFactoryInstance();
  extensions::TtsAPI::GetFactoryInstance();
  extensions::WebNavigationAPI::GetFactoryInstance();
#endif  // defined(ENABLE_EXTENSIONS)
  FaviconServiceFactory::GetInstance();
#if defined(OS_CHROMEOS) && defined(FILE_MANAGER_EXTENSION)
  FileBrowserPrivateAPIFactory::GetInstance();
#endif
  FindBarStateFactory::GetInstance();
  GAIAInfoUpdateServiceFactory::GetInstance();
#if defined(USE_AURA)
  GesturePrefsObserverFactoryAura::GetInstance();
#endif
  GlobalErrorServiceFactory::GetInstance();
  GoogleURLTrackerFactory::GetInstance();
  HistoryServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  notifier::ChromeNotifierServiceFactory::GetInstance();
  MediaGalleriesPreferencesFactory::GetInstance();
#endif
#if defined(OS_CHROMEOS)
  chromeos::NetworkingPrivateEventRouterFactory::GetInstance();
#endif
  NTPResourceCacheFactory::GetInstance();
  PasswordStoreFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
#if !defined(OS_ANDROID)
  PinnedTabServiceFactory::GetInstance();
#endif
#if defined(ENABLE_PLUGINS)
  PluginPrefsFactory::GetInstance();
#endif
  policy::ProfilePolicyConnectorFactory::GetInstance();
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
  policy::UserCloudPolicyManagerFactoryChromeOS::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
#else
  policy::UserCloudPolicyManagerFactory::GetInstance();
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif
#endif
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  predictors::ResourcePrefetchPredictorFactory::GetInstance();
  prerender::PrerenderManagerFactory::GetInstance();
  prerender::PrerenderLinkManagerFactory::GetInstance();
  ProfileSyncServiceFactory::GetInstance();
  ProtocolHandlerRegistryFactory::GetInstance();
#if defined(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
#endif
  ShortcutsBackendFactory::GetInstance();
  ThumbnailServiceFactory::GetInstance();
  SigninManagerFactory::GetInstance();
#if defined(ENABLE_INPUT_SPEECH)
  ChromeSpeechRecognitionPreferences::InitializeFactory();
#endif
  SpellcheckServiceFactory::GetInstance();
  TabRestoreServiceFactory::GetInstance();
  TemplateURLFetcherFactory::GetInstance();
  TemplateURLServiceFactory::GetInstance();
#if defined(ENABLE_THEMES)
  ThemeServiceFactory::GetInstance();
#endif
  TokenCacheServiceFactory::GetInstance();
  TokenServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  UserStyleSheetWatcherFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();

  built_factories_ = true;
}

#ifndef NDEBUG
namespace {

std::string ProfileKeyedBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<ProfileKeyedBaseFactory*>(node)->name();
}

}  // namespace

void ProfileDependencyManager::DumpProfileDependencies(Profile* profile) {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/profile/profile-dependencies.dot".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpProfileDependencyGraph)) {
    base::FilePath dot_file =
        profile->GetPath().AppendASCII("profile-dependencies.dot");
    std::string contents = dependency_graph_.DumpAsGraphviz(
        "Profile", base::Bind(&ProfileKeyedBaseFactoryGetNodeName));
    file_util::WriteFile(dot_file, contents.c_str(), contents.size());
  }
}
#endif  // NDEBUG
