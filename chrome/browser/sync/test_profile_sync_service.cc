// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_core.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/sync/test/test_http_bridge_factory.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/test/sync_manager_factory_for_profile_sync_test.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/protocol/encryption.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using syncer::InternalComponentsFactory;
using syncer::TestInternalComponentsFactory;
using syncer::UserShare;

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
    base::Closure callback)
    : browser_sync::SyncBackendHostImpl(
        profile->GetDebugName(), profile, invalidator,
        sync_prefs, base::FilePath(FILE_PATH_LITERAL("test"))),
      callback_(callback) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

void SyncBackendHostForProfileSyncTest::InitCore(
    scoped_ptr<DoInitializeOptions> options) {
  options->http_bridge_factory =
      scoped_ptr<syncer::HttpPostProviderFactory>(
          new browser_sync::TestHttpBridgeFactory());
  options->sync_manager_factory.reset(
      new syncer::SyncManagerFactoryForProfileSyncTest(callback_));
  options->credentials.email = "testuser@gmail.com";
  options->credentials.sync_token = "token";
  options->credentials.scope_set.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  options->restored_key_for_bootstrapping = "";

  // It'd be nice if we avoided creating the InternalComponentsFactory in the
  // first place, but SyncBackendHost will have created one by now so we must
  // free it. Grab the switches to pass on first.
  InternalComponentsFactory::Switches factory_switches =
      options->internal_components_factory->GetSwitches();
  options->internal_components_factory.reset(
      new TestInternalComponentsFactory(factory_switches,
                                        syncer::STORAGE_IN_MEMORY));

  SyncBackendHostImpl::InitCore(options.Pass());
}

void SyncBackendHostForProfileSyncTest::RequestConfigureSyncer(
    syncer::ConfigureReason reason,
    syncer::ModelTypeSet to_download,
    syncer::ModelTypeSet to_purge,
    syncer::ModelTypeSet to_journal,
    syncer::ModelTypeSet to_unapply,
    syncer::ModelTypeSet to_ignore,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  syncer::ModelTypeSet failed_configuration_types;

  // The first parameter there should be the set of enabled types.  That's not
  // something we have access to from this strange test harness.  We'll just
  // send back the list of newly configured types instead and hope it doesn't
  // break anything.
  FinishConfigureDataTypesOnFrontendLoop(
      syncer::Difference(to_download, failed_configuration_types),
      syncer::Difference(to_download, failed_configuration_types),
      failed_configuration_types,
      ready_task);
}

}  // namespace browser_sync

syncer::TestIdFactory* TestProfileSyncService::id_factory() {
  return &id_factory_;
}

syncer::WeakHandle<syncer::JsEventHandler>
TestProfileSyncService::GetJsEventHandler() {
  return syncer::WeakHandle<syncer::JsEventHandler>();
}

TestProfileSyncService::TestProfileSyncService(
    scoped_ptr<ProfileSyncComponentsFactory> factory,
    Profile* profile,
    SigninManagerBase* signin,
    ProfileOAuth2TokenService* oauth2_token_service,
    browser_sync::ProfileSyncServiceStartBehavior behavior)
    : ProfileSyncService(
          factory.Pass(),
          profile,
          make_scoped_ptr(new SupervisedUserSigninManagerWrapper(profile,
                                                                 signin)),
          oauth2_token_service,
          behavior) {
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {
}

// static
KeyedService* TestProfileSyncService::TestFactoryFunction(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile);
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  return new TestProfileSyncService(
      scoped_ptr<ProfileSyncComponentsFactory>(
          new ProfileSyncComponentsFactoryMock()),
      profile,
      signin,
      oauth2_token_service,
      browser_sync::AUTO_START);
}

// static
TestProfileSyncService* TestProfileSyncService::BuildAutoStartAsyncInit(
    Profile* profile, base::Closure callback) {
  TestProfileSyncService* sync_service = static_cast<TestProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, &TestProfileSyncService::TestFactoryFunction));
  ProfileSyncComponentsFactoryMock* components =
      sync_service->components_factory_mock();
  // TODO(tim): Convert to a fake instead of mock.
  EXPECT_CALL(*components,
              CreateSyncBackendHost(testing::_,testing::_, testing::_,
                                    testing::_, testing::_)).
      WillOnce(testing::Return(
          new browser_sync::SyncBackendHostForProfileSyncTest(
              profile,
              invalidation::ProfileInvalidationProviderFactory::GetForProfile(
                  profile)->GetInvalidationService(),
              sync_service->sync_prefs_.AsWeakPtr(),
              callback)));
  return sync_service;
}

ProfileSyncComponentsFactoryMock*
TestProfileSyncService::components_factory_mock() {
  // We always create a mock factory, see Build* routines.
  return static_cast<ProfileSyncComponentsFactoryMock*>(factory());
}

void TestProfileSyncService::OnConfigureDone(
    const sync_driver::DataTypeManager::ConfigureResult& result) {
  ProfileSyncService::OnConfigureDone(result);
  base::MessageLoop::current()->Quit();
}

UserShare* TestProfileSyncService::GetUserShare() const {
  return backend_->GetUserShare();
}
