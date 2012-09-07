// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/test/test_http_bridge_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/js/js_reply_handler.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/syncable/directory.h"

using syncer::InternalComponentsFactory;
using syncer::ModelSafeRoutingInfo;
using syncer::TestInternalComponentsFactory;
using syncer::sessions::ModelNeutralState;
using syncer::sessions::SyncSessionSnapshot;
using syncer::sessions::SyncSourceInfo;
using syncer::UserShare;
using syncer::syncable::Directory;

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    const base::WeakPtr<SyncPrefs>& sync_prefs,
    const base::WeakPtr<InvalidatorStorage>& invalidator_storage,
    syncer::TestIdFactory& id_factory,
    base::Closure& callback,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init,
    bool fail_initial_download,
    syncer::StorageOption storage_option)
    : browser_sync::SyncBackendHost(
        profile->GetDebugName(), profile, sync_prefs, invalidator_storage),
      id_factory_(id_factory),
      callback_(callback),
      set_initial_sync_ended_on_init_(set_initial_sync_ended_on_init),
      synchronous_init_(synchronous_init),
      fail_initial_download_(fail_initial_download),
      storage_option_(storage_option) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

namespace {

scoped_ptr<syncer::HttpPostProviderFactory> MakeTestHttpBridgeFactory() {
  return scoped_ptr<syncer::HttpPostProviderFactory>(
      new browser_sync::TestHttpBridgeFactory());
}

}  // namespace

void SyncBackendHostForProfileSyncTest::InitCore(
    const DoInitializeOptions& options) {
  DoInitializeOptions test_options = options;
  test_options.make_http_bridge_factory_fn =
      base::Bind(&MakeTestHttpBridgeFactory);
  test_options.credentials.email = "testuser@gmail.com";
  test_options.credentials.sync_token = "token";
  test_options.restored_key_for_bootstrapping = "";
  syncer::StorageOption storage = storage_option_;

  // It'd be nice if we avoided creating the InternalComponentsFactory in the
  // first place, but SyncBackendHost will have created one by now so we must
  // free it. Grab the switches to pass on first.
  InternalComponentsFactory::Switches factory_switches =
      test_options.internal_components_factory->GetSwitches();
  delete test_options.internal_components_factory;

  test_options.internal_components_factory =
      new TestInternalComponentsFactory(factory_switches, storage);

  SyncBackendHost::InitCore(test_options);
  if (synchronous_init_) {
    // The SyncBackend posts a task to the current loop when
    // initialization completes.
    MessageLoop::current()->Run();
  }
}

void SyncBackendHostForProfileSyncTest::RequestConfigureSyncer(
    syncer::ConfigureReason reason,
    syncer::ModelTypeSet types_to_config,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(syncer::ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  syncer::ModelTypeSet failed_configuration_types;
  if (fail_initial_download_)
    failed_configuration_types = types_to_config;

  FinishConfigureDataTypesOnFrontendLoop(failed_configuration_types,
                                         ready_task);
}

void SyncBackendHostForProfileSyncTest
        ::HandleSyncManagerInitializationOnFrontendLoop(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend, bool success,
    syncer::ModelTypeSet restored_types) {
  // Here's our opportunity to pretend to do things that the SyncManager would
  // normally do during initialization, but can't because this is a test.
  if (success) {
    // Set up any nodes the test wants around before model association.
    if (!callback_.is_null()) {
      callback_.Run();
    }

    // Pretend we downloaded initial updates and set initial sync ended bits
    // if we were asked to.
    if (set_initial_sync_ended_on_init_) {
      UserShare* user_share = GetUserShare();
      Directory* directory = user_share->directory.get();

      if (!directory->initial_sync_ended_for_type(syncer::NIGORI)) {
        ProfileSyncServiceTestHelper::CreateRoot(
            syncer::NIGORI, user_share, &id_factory_);

        // A side effect of adding the NIGORI mode (normally done by the
        // syncer) is a decryption attempt, which will fail the first time.
      }

      SetInitialSyncEndedForAllTypes();
      restored_types = syncer::ModelTypeSet::All();
    }
  }

  SyncBackendHost::HandleSyncManagerInitializationOnFrontendLoop(
      js_backend, success, restored_types);
}

void SyncBackendHostForProfileSyncTest::SetInitialSyncEndedForAllTypes() {
  UserShare* user_share = GetUserShare();
  Directory* directory = user_share->directory.get();

  for (int i = syncer::FIRST_REAL_MODEL_TYPE;
       i < syncer::MODEL_TYPE_COUNT; ++i) {
    directory->set_initial_sync_ended_for_type(
        syncer::ModelTypeFromInt(i), true);
  }
}

void SyncBackendHostForProfileSyncTest::EmitOnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  frontend()->OnInvalidatorStateChange(state);
}

void SyncBackendHostForProfileSyncTest::EmitOnIncomingInvalidation(
    const syncer::ObjectIdStateMap& id_state_map,
    const syncer::IncomingInvalidationSource source) {
  frontend()->OnIncomingInvalidation(id_state_map, source);
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

TestProfileSyncService::TestProfileSyncService(
    ProfileSyncComponentsFactory* factory,
    Profile* profile,
    SigninManager* signin,
    ProfileSyncService::StartBehavior behavior,
    bool synchronous_backend_initialization,
    const base::Closure& callback)
    : ProfileSyncService(factory,
                         profile,
                         signin,
                         behavior),
      synchronous_backend_initialization_(
          synchronous_backend_initialization),
      synchronous_sync_configuration_(false),
      callback_(callback),
      set_initial_sync_ended_on_init_(true),
      fail_initial_download_(false),
      storage_option_(syncer::STORAGE_IN_MEMORY) {
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {
}

void TestProfileSyncService::OnBackendInitialized(
    const syncer::WeakHandle<syncer::JsBackend>& backend,
    bool success) {
  ProfileSyncService::OnBackendInitialized(backend, success);

  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_backend_initialization_) {
    MessageLoop::current()->Quit();
  }
}

void TestProfileSyncService::OnConfigureDone(
    const browser_sync::DataTypeManager::ConfigureResult& result) {
  ProfileSyncService::OnConfigureDone(result);
  if (!synchronous_sync_configuration_)
    MessageLoop::current()->Quit();
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
      invalidator_storage_.AsWeakPtr(),
      id_factory_,
      callback_,
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_,
      fail_initial_download_,
      storage_option_));
}
