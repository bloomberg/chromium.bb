// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/browser/profile_sync_components_factory_impl.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/startup_controller.h"
#include "components/sync_driver/sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#endif

namespace {

void UpdateNetworkTimeOnUIThread(base::Time network_time,
                                 base::TimeDelta resolution,
                                 base::TimeDelta latency,
                                 base::TimeTicks post_time) {
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkTime(const base::Time& network_time,
                       const base::TimeDelta& resolution,
                       const base::TimeDelta& latency) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateNetworkTimeOnUIThread, network_time, resolution,
                 latency, base::TimeTicks::Now()));
}

}  // anonymous namespace

// static
ProfileSyncServiceFactory* ProfileSyncServiceFactory::GetInstance() {
  return base::Singleton<ProfileSyncServiceFactory>::get();
}

// static
ProfileSyncService* ProfileSyncServiceFactory::GetForProfile(
    Profile* profile) {
  if (!ProfileSyncService::IsSyncAllowedByFlag())
    return NULL;

  return static_cast<ProfileSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
sync_driver::SyncService*
ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(
    content::BrowserContext* context) {
  return GetForProfile(Profile::FromBrowserContext(context));
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
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
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
  ProfileSyncService::InitParams init_params;

  Profile* profile = Profile::FromBrowserContext(context);

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);

  // Always create the GCMProfileService instance such that we can listen to
  // the profile notifications and purge the GCM store when the profile is
  // being signed out.
  gcm::GCMProfileServiceFactory::GetForProfile(profile);

  // TODO(atwilson): Change AboutSigninInternalsFactory to load on startup
  // once http://crbug.com/171406 has been fixed.
  AboutSigninInternalsFactory::GetForProfile(profile);

  init_params.signin_wrapper =
      base::WrapUnique(new SupervisedUserSigninManagerWrapper(profile, signin));
  init_params.oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  init_params.gaia_cookie_manager_service =
      GaiaCookieManagerServiceFactory::GetForProfile(profile);

  // TODO(tim): Currently, AUTO/MANUAL settings refer to the *first* time sync
  // is set up and *not* a browser restart for a manual-start platform (where
  // sync has already been set up, and should be able to start without user
  // intervention). We can get rid of the browser_default eventually, but
  // need to take care that ProfileSyncService doesn't get tripped up between
  // those two cases. Bug 88109.
  init_params.start_behavior = browser_defaults::kSyncAutoStarts
                                   ? ProfileSyncService::AUTO_START
                                   : ProfileSyncService::MANUAL_START;

  init_params.sync_client =
      base::WrapUnique(new browser_sync::ChromeSyncClient(profile));

  init_params.network_time_update_callback = base::Bind(&UpdateNetworkTime);
  init_params.base_directory = profile->GetPath();
  init_params.url_request_context = profile->GetRequestContext();
  init_params.debug_identifier = profile->GetDebugName();
  init_params.channel = chrome::GetChannel();

  init_params.db_thread = content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::DB);
  init_params.file_thread =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE);
  init_params.blocking_pool = content::BrowserThread::GetBlockingPool();

  auto pss = base::WrapUnique(new ProfileSyncService(std::move(init_params)));

  // Will also initialize the sync client.
  pss->Initialize();
  return pss.release();
}

// static
bool ProfileSyncServiceFactory::HasProfileSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != NULL;
}
