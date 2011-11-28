// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/internal_api/user_share.h"
#include "chrome/browser/sync/js/js_reply_handler.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/test_http_bridge_factory.h"
#include "chrome/common/chrome_notification_types.h"

using browser_sync::ModelSafeRoutingInfo;
using browser_sync::sessions::ErrorCounters;
using browser_sync::sessions::SyncSourceInfo;
using browser_sync::sessions::SyncerStatus;
using browser_sync::sessions::SyncSessionSnapshot;
using syncable::DirectoryManager;
using syncable::ModelType;
using syncable::ScopedDirLookup;
using sync_api::UserShare;

namespace {

class FakeSigninManager : public SigninManager {
 public:
  FakeSigninManager() {}
  virtual ~FakeSigninManager() {}

  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha) OVERRIDE {
    SetUsername(username);
  }
};

}  // namespace

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    const base::WeakPtr<SyncPrefs>& sync_prefs,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init,
    bool fail_initial_download)
    : browser_sync::SyncBackendHost(
        profile->GetDebugName(), profile, sync_prefs),
      synchronous_init_(synchronous_init),
      fail_initial_download_(fail_initial_download) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

void SyncBackendHostForProfileSyncTest::
    SimulateSyncCycleCompletedInitialSyncEnded(
    const tracked_objects::Location& location) {
  syncable::ModelTypeBitSet sync_ended;
  if (!fail_initial_download_)
    sync_ended.set();
  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  core_->HandleSyncCycleCompletedOnFrontendLoop(new SyncSessionSnapshot(
      SyncerStatus(), ErrorCounters(), 0, false,
      sync_ended, download_progress_markers, false, false, 0, 0, 0, false,
      SyncSourceInfo(), 0, base::Time::Now()));
}

sync_api::HttpPostProviderFactory*
    SyncBackendHostForProfileSyncTest::MakeHttpBridgeFactory(
        const scoped_refptr<net::URLRequestContextGetter>& getter) {
  return new browser_sync::TestHttpBridgeFactory;
}

void SyncBackendHostForProfileSyncTest::InitCore(
    const Core::DoInitializeOptions& options) {
  Core::DoInitializeOptions test_options = options;
  test_options.credentials.email = "testuser@gmail.com";
  test_options.credentials.sync_token = "token";
  test_options.restored_key_for_bootstrapping = "";
  test_options.setup_for_test_mode = true;
  SyncBackendHost::InitCore(test_options);
  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_init_) {
    // The SyncBackend posts a task to the current loop when
    // initialization completes.
    MessageLoop::current()->Run();
  }
}

void SyncBackendHostForProfileSyncTest::StartConfiguration(
    const base::Closure& callback) {
  SyncBackendHost::FinishConfigureDataTypesOnFrontendLoop();
  if (initialization_state_ == DOWNLOADING_NIGORI) {
    syncable::ModelTypeBitSet sync_ended;

    if (!fail_initial_download_)
      sync_ended.set(syncable::NIGORI);
    std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
    core_->HandleSyncCycleCompletedOnFrontendLoop(new SyncSessionSnapshot(
        SyncerStatus(), ErrorCounters(), 0, false,
        sync_ended, download_progress_markers, false, false, 0, 0, 0, false,
        SyncSourceInfo(), 0, base::Time::Now()));
  }
}

void SyncBackendHostForProfileSyncTest::
    SetDefaultExpectationsForWorkerCreation(ProfileMock* profile) {
  EXPECT_CALL(*profile, GetPasswordStore(testing::_)).
      WillOnce(testing::Return((PasswordStore*)NULL));
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
    const std::string& test_user,
    bool synchronous_backend_initialization,
    const base::Closure& callback)
    : ProfileSyncService(factory, profile, new FakeSigninManager(), test_user),
      synchronous_backend_initialization_(
          synchronous_backend_initialization),
      synchronous_sync_configuration_(false),
      callback_(callback),
      set_initial_sync_ended_on_init_(true),
      fail_initial_download_(false) {
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {}

void TestProfileSyncService::SetInitialSyncEndedForAllTypes() {
  UserShare* user_share = GetUserShare();
  DirectoryManager* dir_manager = user_share->dir_manager.get();

  ScopedDirLookup dir(dir_manager, user_share->name);
  if (!dir.good())
    FAIL();

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    dir->set_initial_sync_ended_for_type(
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
      DirectoryManager* dir_manager = user_share->dir_manager.get();

      ScopedDirLookup dir(dir_manager, user_share->name);
      if (!dir.good())
        FAIL();

      if (!dir->initial_sync_ended_for_type(syncable::NIGORI)) {
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
    OnPassphraseRequired(sync_api::REASON_DECRYPTION);

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

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
      profile(),
      sync_prefs_.AsWeakPtr(),
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_,
      fail_initial_download_));
}
