// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"

#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/notifications/extension_welcome_notification_factory.h"
#include "chrome/browser/notifications/notification_permission_context_factory.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/password_manager/password_manager_setting_migrator_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_message_filter.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"

#if defined(ENABLE_EXTENSIONS)
#include "apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/ephemeral_app_service_factory.h"
#include "chrome/browser/apps/shortcut_manager_factory.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_factory_impl.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_verify_delegate_factory_impl.h"
#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/browser/ui/bookmarks/enhanced_bookmark_key_service_factory.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
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
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/consumer_enrollment_handler_factory.h"
#include "chrome/browser/chromeos/policy/consumer_management_notifier_factory.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/recommendation_restorer_factory.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/supervised_user/chromeos/manager_password_service_factory.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service_factory.h"
#endif
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#else
#include "chrome/browser/signin/cross_device_promo_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/printer_detector/printer_detector_factory.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
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
  extensions::ExtensionManagementFactory::GetInstance();
  chrome_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  AppShortcutManagerFactory::GetInstance();
  EphemeralAppServiceFactory::GetInstance();
#endif

#if defined(ENABLE_APP_LIST)
  app_list::AppListSyncableServiceFactory::GetInstance();
#endif

  AboutSigninInternalsFactory::GetInstance();
  AccountTrackerServiceFactory::GetInstance();
  AccountFetcherServiceFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
#if !defined(OS_ANDROID)
  AutomaticProfileResetterFactory::GetInstance();
#endif
#if defined(ENABLE_BACKGROUND)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
  BookmarkUndoServiceFactory::GetInstance();
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
  GeolocationPermissionContextFactory::GetInstance();
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  ProtectedMediaIdentifierPermissionContextFactory::GetInstance();
#endif
#if defined(ENABLE_PRINT_PREVIEW)
  CloudPrintProxyServiceFactory::GetInstance();
#endif
  CookieSettingsFactory::GetInstance();
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  CrossDevicePromoFactory::GetInstance();
#endif
#if defined(ENABLE_NOTIFICATIONS)
#if defined(ENABLE_EXTENSIONS)
  ExtensionWelcomeNotificationFactory::GetInstance();
#endif
  NotificationPermissionContextFactory::GetInstance();
  NotifierStateTrackerFactory::GetInstance();
#endif  // defined(ENABLE_NOTIFICATIONS)
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  domain_reliability::DomainReliabilityServiceFactory::GetInstance();
  DownloadServiceFactory::GetInstance();
#if defined(ENABLE_EXTENSIONS)
  EasyUnlockServiceFactory::GetInstance();
  EnhancedBookmarkKeyServiceFactory::GetInstance();
#endif
#if defined(OS_CHROMEOS)
  chromeos::PrinterDetectorFactory::GetInstance();
  extensions::VerifyTrustAPI::GetFactoryInstance();
#endif
  FaviconServiceFactory::GetInstance();
  FindBarStateFactory::GetInstance();
  GAIAInfoUpdateServiceFactory::GetInstance();
#if defined(USE_AURA)
  GesturePrefsObserverFactoryAura::GetInstance();
#endif
#if !defined(OS_ANDROID)
  GlobalErrorServiceFactory::GetInstance();
#endif
  GoogleURLTrackerFactory::GetInstance();
  HistoryServiceFactory::GetInstance();
#if defined(ENABLE_EXTENSIONS)
  HotwordServiceFactory::GetInstance();
#endif
  HostContentSettingsMapFactory::GetInstance();
  InMemoryURLIndexFactory::GetInstance();
  invalidation::ProfileInvalidationProviderFactory::GetInstance();
  InstantServiceFactory::GetInstance();
#if defined(ENABLE_SERVICE_DISCOVERY)
  local_discovery::PrivetNotificationServiceFactory::GetInstance();
#endif
#if defined(ENABLE_SUPERVISED_USERS)
#if defined(OS_CHROMEOS)
  chromeos::SupervisedUserPasswordServiceFactory::GetInstance();
  chromeos::ManagerPasswordServiceFactory::GetInstance();
#endif
  SupervisedUserServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  SupervisedUserSyncServiceFactory::GetInstance();
#endif
#endif
#if defined(ENABLE_EXTENSIONS)
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
  scoped_ptr<extensions::NetworkingPrivateVerifyDelegateFactoryImpl>
      networking_private_verify_delegate_factory(
          new extensions::NetworkingPrivateVerifyDelegateFactoryImpl);
  extensions::NetworkingPrivateDelegateFactory::GetInstance()
      ->SetVerifyDelegateFactory(
          networking_private_verify_delegate_factory.Pass());
  scoped_ptr<extensions::NetworkingPrivateUIDelegateFactoryImpl>
      networking_private_ui_delegate_factory(
          new extensions::NetworkingPrivateUIDelegateFactoryImpl);
  extensions::NetworkingPrivateDelegateFactory::GetInstance()
      ->SetUIDelegateFactory(networking_private_ui_delegate_factory.Pass());
#endif
#endif
#if !defined(OS_ANDROID)
  MediaGalleriesPreferencesFactory::GetInstance();
  NTPResourceCacheFactory::GetInstance();
#endif
  PasswordStoreFactory::GetInstance();
  PasswordManagerSettingMigratorServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  PinnedTabServiceFactory::GetInstance();
#endif
#if defined(ENABLE_PLUGINS)
  PluginPrefsFactory::GetInstance();
#endif
  PrefsTabHelper::GetServiceInstance();
  policy::ProfilePolicyConnectorFactory::GetInstance();
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOSFactory::GetInstance();
  policy::ConsumerEnrollmentHandlerFactory::GetInstance();
  policy::ConsumerManagementNotifierFactory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::RecommendationRestorerFactory::GetInstance();
  policy::UserCloudPolicyManagerFactoryChromeOS::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
#else
  policy::UserCloudPolicyManagerFactory::GetInstance();
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif
  policy::PolicyHeaderServiceFactory::GetInstance();
  policy::SchemaRegistryServiceFactory::GetInstance();
  policy::UserCloudPolicyInvalidatorFactory::GetInstance();
#endif
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  predictors::ResourcePrefetchPredictorFactory::GetInstance();
  prerender::PrerenderLinkManagerFactory::GetInstance();
  prerender::PrerenderManagerFactory::GetInstance();
  prerender::PrerenderMessageFilter::EnsureShutdownNotifierFactoryBuilt();
  ProfileSyncServiceFactory::GetInstance();
  ProtocolHandlerRegistryFactory::GetInstance();
#if defined(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
#endif
  ShortcutsBackendFactory::GetInstance();
  SigninManagerFactory::GetInstance();

  if (SiteEngagementService::IsEnabled())
    SiteEngagementServiceFactory::GetInstance();

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
#if defined(OS_WIN)
  TriggeredProfileResetterFactory::GetInstance();
#endif
#if !defined(OS_ANDROID)
  UsbChooserContextFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();
}

void ChromeBrowserMainExtraPartsProfiles::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}
