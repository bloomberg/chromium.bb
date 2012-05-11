// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"

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
      GetInstance()->GetServiceForProfile(profile, true));
}

ProfileSyncServiceFactory::ProfileSyncServiceFactory()
    : ProfileKeyedServiceFactory("ProfileSyncService",
                                 ProfileDependencyManager::GetInstance()) {

  // The ProfileSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(PersonalDataManagerFactory::GetInstance());
#if defined(ENABLE_THEMES)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());

  // The following have not been converted to ProfileKeyedServices yet, and for
  // now they are explicitly destroyed after the ProfileDependencyManager is
  // told to DestroyProfileServices, so they will be around when the
  // ProfileSyncService is destroyed.

  // DependsOn(HistoryServiceFactory::GetInstance());
  // DependsOn(BookmarkBarModelFactory::GetInstance());
  // DependsOn(FaviconServiceFactory::GetInstance());
}

ProfileSyncServiceFactory::~ProfileSyncServiceFactory() {
}

ProfileKeyedService* ProfileSyncServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  ProfileSyncService::StartBehavior behavior =
      browser_defaults::kSyncAutoStarts ? ProfileSyncService::AUTO_START
                                        : ProfileSyncService::MANUAL_START;

  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);

  // TODO(tim): Currently, AUTO/MANUAL settings refer to the *first* time sync
  // is set up and *not* a browser restart for a manual-start platform (where
  // sync has already been set up, and should be able to start without user
  // intervention). We can get rid of the browser_default eventually, but
  // need to take care that ProfileSyncService doesn't get tripped up between
  // those two cases. Bug 88109.
  ProfileSyncService* pss = new ProfileSyncService(
      new ProfileSyncComponentsFactoryImpl(profile,
                                           CommandLine::ForCurrentProcess()),
      profile,
      signin,
      behavior);

  pss->factory()->RegisterDataTypes(pss);
  pss->Initialize();
  return pss;
}

// static
bool ProfileSyncServiceFactory::HasProfileSyncService(Profile* profile) {
  return GetInstance()->GetServiceForProfile(profile, false) != NULL;
}
