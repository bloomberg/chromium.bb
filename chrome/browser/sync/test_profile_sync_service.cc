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
#include "sync/syncable/syncable.h"

using browser_sync::ModelSafeRoutingInfo;
using browser_sync::sessions::ErrorCounters;
using browser_sync::sessions::SyncSourceInfo;
using browser_sync::sessions::SyncerStatus;
using browser_sync::sessions::SyncSessionSnapshot;
using syncable::Directory;
using syncable::ModelType;
using sync_api::UserShare;

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    const base::WeakPtr<SyncPrefs>& sync_prefs,
    const base::WeakPtr<InvalidatorStorage>& invalidator_storage,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init,
    bool fail_initial_download,
    bool use_real_database)
    : browser_sync::SyncBackendHost(
        profile->GetDebugName(), profile, sync_prefs, invalidator_storage),
      synchronous_init_(synchronous_init),
      fail_initial_download_(fail_initial_download),
      use_real_database_(use_real_database) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

namespace {

sync_api::HttpPostProviderFactory* MakeTestHttpBridgeFactory() {
  return new browser_sync::TestHttpBridgeFactory();
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
  test_options.testing_mode =
      use_real_database_ ? sync_api::SyncManager::TEST_ON_DISK
                         : sync_api::SyncManager::TEST_IN_MEMORY;
  SyncBackendHost::InitCore(test_options);
  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_init_) {
    // The SyncBackend posts a task to the current loop when
    // initialization completes.
    MessageLoop::current()->Run();
  }
}

void SyncBackendHostForProfileSyncTest::RequestConfigureSyncer(
    sync_api::ConfigureReason reason,
    syncable::ModelTypeSet types_to_config,
    const browser_sync::ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(syncable::ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  syncable::ModelTypeSet sync_ended;
  if (!fail_initial_download_)
    sync_ended.PutAll(types_to_config);

  FinishConfigureDataTypesOnFrontendLoop(types_to_config,
                                         sync_ended,
                                         ready_task);
}

void SyncBackendHostForProfileSyncTest::SetHistoryServiceExpectations(
    ProfileMock* profile) {
  EXPECT_CALL(*profile, GetHistoryService(testing::_)).
      WillOnce(testing::Return((HistoryService*)NULL));
}

}  // namespace browser_sync

browser_sync::TestIdFactory* TestProfileSyncService::id_factory() {
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
      use_real_database_(false) {
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {
}

void TestProfileSyncService::SetInitialSyncEndedForAllTypes() {
  UserShare* user_share = GetUserShare();
  Directory* directory = user_share->directory.get();

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    directory->set_initial_sync_ended_for_type(
        syncable::ModelTypeFromInt(i), true);
  }
}

void TestProfileSyncService::OnBackendInitialized(
    const browser_sync::WeakHandle<browser_sync::JsBackend>& backend,
    bool success) {
  bool send_passphrase_required = false;
  if (success) {
    // Set this so below code can access GetUserShare().
    backend_initialized_ = true;

    // Set up any nodes the test wants around before model association.
    if (!callback_.is_null()) {
      callback_.Run();
      callback_.Reset();
    }

    // Pretend we downloaded initial updates and set initial sync ended bits
    // if we were asked to.
    if (set_initial_sync_ended_on_init_) {
      UserShare* user_share = GetUserShare();
      Directory* directory = user_share->directory.get();

      if (!directory->initial_sync_ended_for_type(syncable::NIGORI)) {
        ProfileSyncServiceTestHelper::CreateRoot(
            syncable::NIGORI, GetUserShare(),
            id_factory());

        // A side effect of adding the NIGORI mode (normally done by the
        // syncer) is a decryption attempt, which will fail the first time.
        send_passphrase_required = true;
      }

      SetInitialSyncEndedForAllTypes();
    }
  }

  ProfileSyncService::OnBackendInitialized(backend, success);
  if (success && send_passphrase_required)
    OnPassphraseRequired(sync_api::REASON_DECRYPTION, sync_pb::EncryptedData());

  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_backend_initialization_) {
    MessageLoop::current()->Quit();
  }
}

void TestProfileSyncService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ProfileSyncService::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_SYNC_CONFIGURE_DONE &&
      !synchronous_sync_configuration_) {
    MessageLoop::current()->Quit();
  }
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
void TestProfileSyncService::set_use_real_database() {
  use_real_database_ = true;
}

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
      profile(),
      sync_prefs_.AsWeakPtr(),
      invalidator_storage_.AsWeakPtr(),
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_,
      fail_initial_download_,
      use_real_database_));
}
