// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/startup_controller.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

// static
ProfileSyncServiceFactory* ProfileSyncServiceFactory::GetInstance() {
  return Singleton<ProfileSyncServiceFactory>::get();
}

// static
ProfileSyncService* ProfileSyncServiceFactory::GetForProfile(
    Profile* profile) {
  if (!ProfileSyncService::IsSyncEnabled())
    return NULL;

  return static_cast<ProfileSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ProfileSyncServiceFactory::ProfileSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileSyncService",
        BrowserContextDependencyManager::GetInstance()) {
  // The ProfileSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(AboutSigninInternalsFactory::GetInstance());
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if defined(ENABLE_THEMES)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
  DependsOn(WebDataServiceFactory::GetInstance());
#if defined(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(notifier::ChromeNotifierServiceFactory::GetInstance());
#endif

  // The following have not been converted to KeyedServices yet,
  // and for now they are explicitly destroyed after the
  // BrowserContextDependencyManager is told to DestroyBrowserContextServices,
  // so they will be around when the ProfileSyncService is destroyed.

  // DependsOn(FaviconServiceFactory::GetInstance());
}

ProfileSyncServiceFactory::~ProfileSyncServiceFactory() {
}

KeyedService* ProfileSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);

  // Always create the GCMProfileService instance such that we can listen to
  // the profile notifications and purge the GCM store when the profile is
  // being signed out.
  gcm::GCMProfileServiceFactory::GetForProfile(profile);

  // TODO(atwilson): Change AboutSigninInternalsFactory to load on startup
  // once http://crbug.com/171406 has been fixed.
  AboutSigninInternalsFactory::GetForProfile(profile);

  const GURL sync_service_url =
      ProfileSyncService::GetSyncServiceURL(*CommandLine::ForCurrentProcess());

  scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper(
      new SupervisedUserSigninManagerWrapper(profile, signin));
  std::string account_id = signin_wrapper->GetAccountIdToUse();
  OAuth2TokenService::ScopeSet scope_set;
  scope_set.insert(signin_wrapper->GetSyncScopeToUse());
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  net::URLRequestContextGetter* url_request_context_getter =
      profile->GetRequestContext();

  // TODO(tim): Currently, AUTO/MANUAL settings refer to the *first* time sync
  // is set up and *not* a browser restart for a manual-start platform (where
  // sync has already been set up, and should be able to start without user
  // intervention). We can get rid of the browser_default eventually, but
  // need to take care that ProfileSyncService doesn't get tripped up between
  // those two cases. Bug 88109.
  browser_sync::ProfileSyncServiceStartBehavior behavior =
      browser_defaults::kSyncAutoStarts ? browser_sync::AUTO_START
                                        : browser_sync::MANUAL_START;
  ProfileSyncService* pss = new ProfileSyncService(
      scoped_ptr<ProfileSyncComponentsFactory>(
          new ProfileSyncComponentsFactoryImpl(
              profile,
              CommandLine::ForCurrentProcess(),
              sync_service_url,
              token_service,
              url_request_context_getter)),
      profile,
      signin_wrapper.Pass(),
      token_service,
      behavior);

  pss->factory()->RegisterDataTypes(pss);
  pss->Initialize();
  return pss;
}

// static
bool ProfileSyncServiceFactory::HasProfileSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != NULL;
}
