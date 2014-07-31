// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"

#if defined(ENABLE_EXTENSIONS)
#include "apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/ephemeral_app_service_factory.h"
#include "chrome/browser/apps/shortcut_manager_factory.h"
#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator_factory.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/recommendation_restorer_factory.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#if !defined(OS_IOS)
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif
#endif
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/supervised_user/chromeos/manager_password_service_factory.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service_factory.h"
#endif
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#else
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/local_discovery/privet_notifications_factory.h"
#endif

namespace chrome {

void AddProfilesExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsProfiles());
}

}  // namespace chrome

ChromeBrowserMainExtraPartsProfiles::ChromeBrowserMainExtraPartsProfiles() {
}

ChromeBrowserMainExtraPartsProfiles::~ChromeBrowserMainExtraPartsProfiles() {
}

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes itself and registers its dependencies with
// the global PreferenceDependencyManager. We need to have a complete
// dependency graph when we create a profile so we can dispatch the profile
// creation message to the services that want to create their services at
// profile creation time.
//
// TODO(erg): This needs to be something else. I don't think putting every
// FooServiceFactory here will scale or is desirable long term.
//
// static
void ChromeBrowserMainExtraPartsProfiles::
EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if defined(ENABLE_EXTENSIONS)
  apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  AppShortcutManagerFactory::GetInstance();
  EphemeralAppServiceFactory::GetInstance();
#endif

#if defined(ENABLE_APP_LIST)
  app_list::AppListSyncableServiceFactory::GetInstance();
#endif

  AboutSigninInternalsFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
#if !defined(OS_ANDROID)
  AutomaticProfileResetterFactory::GetInstance();
#endif
#if defined(ENABLE_BACKGROUND)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
#if !defined(OS_ANDROID)
  BookmarkUndoServiceFactory::GetInstance();
#endif
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
  GeolocationPermissionContextFactory::GetInstance();
#if defined(OS_ANDROID)
  ProtectedMediaIdentifierPermissionContextFactory::GetInstance();
#endif
#if defined(ENABLE_FULL_PRINTING)
  CloudPrintProxyServiceFactory::GetInstance();
#endif
  CookieSettings::Factory::GetInstance();
#if defined(ENABLE_NOTIFICATIONS)
  DesktopNotificationServiceFactory::GetInstance();
#endif
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  domain_reliability::DomainReliabilityServiceFactory::GetInstance();
  DownloadServiceFactory::GetInstance();
  EasyUnlockServiceFactory::GetInstance();
  FaviconServiceFactory::GetInstance();
  FindBarStateFactory::GetInstance();
  GAIAInfoUpdateServiceFactory::GetInstance();
#if defined(USE_AURA)
  GesturePrefsObserverFactoryAura::GetInstance();
#endif
  GlobalErrorServiceFactory::GetInstance();
  GoogleURLTrackerFactory::GetInstance();
  HistoryServiceFactory::GetInstance();
#if defined(ENABLE_EXTENSIONS)
  HotwordServiceFactory::GetInstance();
#endif
  invalidation::ProfileInvalidationProviderFactory::GetInstance();
  InstantServiceFactory::GetInstance();
#if defined(ENABLE_SERVICE_DISCOVERY)
  local_discovery::PrivetNotificationServiceFactory::GetInstance();
#endif
#if defined(ENABLE_MANAGED_USERS)
#if defined(OS_CHROMEOS)
  chromeos::SupervisedUserPasswordServiceFactory::GetInstance();
  chromeos::ManagerPasswordServiceFactory::GetInstance();
#endif
  SupervisedUserServiceFactory::GetInstance();
  SupervisedUserSyncServiceFactory::GetInstance();
#endif
#if !defined(OS_ANDROID)
  MediaGalleriesPreferencesFactory::GetInstance();
  notifier::ChromeNotifierServiceFactory::GetInstance();
  NTPResourceCacheFactory::GetInstance();
#endif
  PasswordStoreFactory::GetInstance();
#if !defined(OS_ANDROID)
  PinnedTabServiceFactory::GetInstance();
#endif
#if defined(ENABLE_PLUGINS)
  PluginPrefsFactory::GetInstance();
#endif
  policy::ProfilePolicyConnectorFactory::GetInstance();
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceFactory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::RecommendationRestorerFactory::GetInstance();
  policy::UserCloudPolicyManagerFactoryChromeOS::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
#else
  policy::UserCloudPolicyManagerFactory::GetInstance();
#if !defined(OS_IOS)
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif
#endif
  policy::PolicyHeaderServiceFactory::GetInstance();
  policy::SchemaRegistryServiceFactory::GetInstance();
  policy::UserCloudPolicyInvalidatorFactory::GetInstance();
#endif
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  prerender::PrerenderManagerFactory::GetInstance();
  prerender::PrerenderLinkManagerFactory::GetInstance();
  ProfileSyncServiceFactory::GetInstance();
  ProtocolHandlerRegistryFactory::GetInstance();
#if defined(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
#endif
  ShortcutsBackendFactory::GetInstance();
  SigninManagerFactory::GetInstance();
#if defined(ENABLE_SPELLCHECK)
  SpellcheckServiceFactory::GetInstance();
#endif
  suggestions::SuggestionsServiceFactory::GetInstance();
  ThumbnailServiceFactory::GetInstance();
  TabRestoreServiceFactory::GetInstance();
  TemplateURLFetcherFactory::GetInstance();
  TemplateURLServiceFactory::GetInstance();
#if defined(ENABLE_THEMES)
  ThemeServiceFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();
}

void ChromeBrowserMainExtraPartsProfiles::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}
