// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/test/test_http_bridge_factory.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/js/js_reply_handler.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/syncable/directory.h"

using syncer::InternalComponentsFactory;
using syncer::ModelSafeRoutingInfo;
using syncer::TestInternalComponentsFactory;
using syncer::sessions::ModelNeutralState;
using syncer::sessions::SyncSessionSnapshot;
using syncer::UserShare;
using syncer::syncable::Directory;
using syncer::DEVICE_INFO;
using syncer::EXPERIMENTS;
using syncer::NIGORI;
using syncer::PRIORITY_PREFERENCES;

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    const base::WeakPtr<SyncPrefs>& sync_prefs,
    syncer::TestIdFactory& id_factory,
    base::Closure& callback,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init,
    bool fail_initial_download,
    syncer::StorageOption storage_option)
    : browser_sync::SyncBackendHost(
        profile->GetDebugName(), profile, sync_prefs),
      weak_ptr_factory_(this),
      id_factory_(id_factory),
      callback_(callback),
      fail_initial_download_(fail_initial_download),
      set_initial_sync_ended_on_init_(set_initial_sync_ended_on_init),
      synchronous_init_(synchronous_init),
      storage_option_(storage_option) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

namespace {

scoped_ptr<syncer::HttpPostProviderFactory> MakeTestHttpBridgeFactory() {
  return scoped_ptr<syncer::HttpPostProviderFactory>(
      new browser_sync::TestHttpBridgeFactory());
}

}  // namespace

void SyncBackendHostForProfileSyncTest::InitCore(
    scoped_ptr<DoInitializeOptions> options) {
  options->http_bridge_factory = MakeTestHttpBridgeFactory();
  options->credentials.email = "testuser@gmail.com";
  options->credentials.sync_token = "token";
  options->restored_key_for_bootstrapping = "";
  syncer::StorageOption storage = storage_option_;

  // It'd be nice if we avoided creating the InternalComponentsFactory in the
  // first place, but SyncBackendHost will have created one by now so we must
  // free it. Grab the switches to pass on first.
  InternalComponentsFactory::Switches factory_switches =
      options->internal_components_factory->GetSwitches();
  options->internal_components_factory.reset(
      new TestInternalComponentsFactory(factory_switches, storage));

  SyncBackendHost::InitCore(options.Pass());
  if (synchronous_init_ && !base::MessageLoop::current()->is_running()) {
    // The SyncBackend posts a task to the current loop when
    // initialization completes.
    base::MessageLoop::current()->Run();
  }
}

void SyncBackendHostForProfileSyncTest::UpdateCredentials(
      const syncer::SyncCredentials& credentials) {
  // If we had failed the initial download, complete initialization now.
  if (!initial_download_closure_.is_null()) {
    initial_download_closure_.Run();
    initial_download_closure_.Reset();
  }
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
  if (fail_initial_download_)
    failed_configuration_types = to_download;

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

void SyncBackendHostForProfileSyncTest
    ::HandleSyncManagerInitializationOnFrontendLoop(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    syncer::ModelTypeSet restored_types) {
  // Here's our opportunity to pretend to do things that the SyncManager would
  // normally do during initialization, but can't because this is a test.
  // Set up any nodes the test wants around before model association.
  if (!callback_.is_null()) {
    callback_.Run();
  }

  // Pretend we downloaded initial updates and set initial sync ended bits
  // if we were asked to.
  if (set_initial_sync_ended_on_init_) {
    UserShare* user_share = GetUserShare();
    Directory* directory = user_share->directory.get();

    if (!directory->InitialSyncEndedForType(NIGORI)) {
      syncer::TestUserShare::CreateRoot(NIGORI, user_share);

      // A side effect of adding the NIGORI node (normally done by the
      // syncer) is a decryption attempt, which will fail the first time.
    }

    if (!directory->InitialSyncEndedForType(DEVICE_INFO)) {
      syncer::TestUserShare::CreateRoot(DEVICE_INFO, user_share);
    }

    if (!directory->InitialSyncEndedForType(EXPERIMENTS)) {
      syncer::TestUserShare::CreateRoot(EXPERIMENTS, user_share);
    }

    if (!directory->InitialSyncEndedForType(PRIORITY_PREFERENCES)) {
      syncer::TestUserShare::CreateRoot(PRIORITY_PREFERENCES, user_share);
    }

    restored_types = syncer::ModelTypeSet::All();
  }

  initial_download_closure_ = base::Bind(
      &SyncBackendHostForProfileSyncTest::ContinueInitialization,
      weak_ptr_factory_.GetWeakPtr(),
      js_backend,
      debug_info_listener,
      restored_types);
  if (fail_initial_download_) {
    frontend()->OnSyncConfigureRetry();
    if (synchronous_init_)
      base::MessageLoop::current()->Quit();
  } else {
    initial_download_closure_.Run();
    initial_download_closure_.Reset();
  }
}

void SyncBackendHostForProfileSyncTest::ContinueInitialization(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    syncer::ModelTypeSet restored_types) {
  SyncBackendHost::HandleSyncManagerInitializationOnFrontendLoop(
      js_backend, debug_info_listener, restored_types);
}

}  // namespace browser_sync

syncer::TestIdFactory* TestProfileSyncService::id_factory() {
  return &id_factory_;
}

browser_sync::SyncBackendHostForProfileSyncTest*
    TestProfileSyncService::GetBackendForTest() {
  return static_cast<browser_sync::SyncBackendHostForProfileSyncTest*>(
      ProfileSyncService::GetBackendForTest());
}

syncer::WeakHandle<syncer::JsEventHandler>
TestProfileSyncService::GetJsEventHandler() {
  return syncer::WeakHandle<syncer::JsEventHandler>();
}

TestProfileSyncService::TestProfileSyncService(
    ProfileSyncComponentsFactory* factory,
    Profile* profile,
    SigninManagerBase* signin,
    OAuth2TokenService* oauth2_token_service,
    ProfileSyncService::StartBehavior behavior,
    bool synchronous_backend_initialization)
        : ProfileSyncService(factory,
                             profile,
                             signin,
                             oauth2_token_service,
                             behavior),
    synchronous_backend_initialization_(
        synchronous_backend_initialization),
    synchronous_sync_configuration_(false),
    set_initial_sync_ended_on_init_(true),
    fail_initial_download_(false),
    storage_option_(syncer::STORAGE_IN_MEMORY) {
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {
}

// static
BrowserContextKeyedService* TestProfileSyncService::BuildAutoStartAsyncInit(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  return new TestProfileSyncService(factory,
                                    profile,
                                    signin,
                                    oauth2_token_service,
                                    ProfileSyncService::AUTO_START,
                                    false);
}

ProfileSyncComponentsFactoryMock*
TestProfileSyncService::components_factory_mock() {
  // We always create a mock factory, see Build* routines.
  return static_cast<ProfileSyncComponentsFactoryMock*>(factory());
}

void TestProfileSyncService::RequestAccessToken() {
  ProfileSyncService::RequestAccessToken();
  if (synchronous_backend_initialization_) {
    base::MessageLoop::current()->Run();
  }
}

void TestProfileSyncService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  ProfileSyncService::OnGetTokenFailure(request, error);
  if (synchronous_backend_initialization_) {
    base::MessageLoop::current()->Quit();
  }
}


void TestProfileSyncService::OnBackendInitialized(
    const syncer::WeakHandle<syncer::JsBackend>& backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success) {
  ProfileSyncService::OnBackendInitialized(backend,
                                           debug_info_listener,
                                           success);

  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_backend_initialization_) {
    base::MessageLoop::current()->Quit();
  }
}

void TestProfileSyncService::OnConfigureDone(
    const browser_sync::DataTypeManager::ConfigureResult& result) {
  ProfileSyncService::OnConfigureDone(result);
  if (!synchronous_sync_configuration_)
    base::MessageLoop::current()->Quit();
}

UserShare* TestProfileSyncService::GetUserShare() const {
  return backend_->GetUserShare();
}

void TestProfileSyncService::dont_set_initial_sync_ended_on_init() {
  set_initial_sync_ended_on_init_ = false;
}
void TestProfileSyncService::set_synchronous_sync_configuration() {
  synchronous_sync_configuration_ = true;
}
void TestProfileSyncService::fail_initial_download() {
  fail_initial_download_ = true;
}
void TestProfileSyncService::set_storage_option(
    syncer::StorageOption storage_option) {
  storage_option_ = storage_option;
}

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
      profile(),
      sync_prefs_.AsWeakPtr(),
      id_factory_,
      callback_,
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_,
      fail_initial_download_,
      storage_option_));
}

scoped_ptr<OAuth2TokenService::Request> FakeOAuth2TokenService::StartRequest(
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  // Ensure token in question is cached and never expires. Request will succeed
  // without network IO.
  RegisterCacheEntry("test_client_id",
                     GetRefreshToken(),
                     scopes,
                     "access_token",
                     base::Time::Max());
  return ProfileOAuth2TokenService::StartRequest(scopes, consumer);
}

BrowserContextKeyedService* FakeOAuth2TokenService::BuildTokenService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  FakeOAuth2TokenService* service = new FakeOAuth2TokenService();
  service->Initialize(profile);
  return service;
}
