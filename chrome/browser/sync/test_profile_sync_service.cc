// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/test/sync/test_http_bridge_factory.h"

using browser_sync::ModelSafeRoutingInfo;
using browser_sync::sessions::ErrorCounters;
using browser_sync::sessions::SyncSourceInfo;
using browser_sync::sessions::SyncerStatus;
using browser_sync::sessions::SyncSessionSnapshot;
using syncable::DirectoryManager;
using syncable::ModelType;
using syncable::ScopedDirLookup;
using sync_api::UserShare;

ACTION_P(CallOnPaused, core) {
  core->OnPaused();
};

ACTION_P(CallOnResumed, core) {
  core->OnResumed();
}

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    Profile* profile,
    int num_expected_resumes,
    int num_expected_pauses,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init)
    : browser_sync::SyncBackendHost(profile),
      synchronous_init_(synchronous_init) {
  // By default, the RequestPause and RequestResume methods will
  // send the confirmation notification and return true.
  ON_CALL(*this, RequestPause()).
      WillByDefault(testing::DoAll(CallOnPaused(core_),
                                   testing::Return(true)));
  ON_CALL(*this, RequestResume()).
      WillByDefault(testing::DoAll(CallOnResumed(core_),
                                   testing::Return(true)));
  ON_CALL(*this, RequestNudge()).WillByDefault(
      testing::Invoke(this,
                      &SyncBackendHostForProfileSyncTest::
                      SimulateSyncCycleCompletedInitialSyncEnded));

  EXPECT_CALL(*this, RequestPause()).Times(num_expected_pauses);
  EXPECT_CALL(*this, RequestResume()).Times(num_expected_resumes);
  EXPECT_CALL(*this,
              RequestNudge()).Times(set_initial_sync_ended_on_init ? 0 : 1);
}

void SyncBackendHostForProfileSyncTest::ConfigureDataTypes(
    const DataTypeController::TypeMap& data_type_controllers,
    const syncable::ModelTypeSet& types,
    CancelableTask* ready_task) {
  SetAutofillMigrationState(syncable::MIGRATED);
  SyncBackendHost::ConfigureDataTypes(
      data_type_controllers, types, ready_task);
}

void SyncBackendHostForProfileSyncTest::
    SimulateSyncCycleCompletedInitialSyncEnded() {
  syncable::ModelTypeBitSet sync_ended;
  ModelSafeRoutingInfo enabled_types;
  GetModelSafeRoutingInfo(&enabled_types);
  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
       i != enabled_types.end(); ++i) {
    sync_ended.set(i->first);
  }
  core_->HandleSyncCycleCompletedOnFrontendLoop(new SyncSessionSnapshot(
      SyncerStatus(), ErrorCounters(), 0, false,
      sync_ended, download_progress_markers, false, false, 0, 0, false,
      SyncSourceInfo()));
}

sync_api::HttpPostProviderFactory*
    SyncBackendHostForProfileSyncTest::MakeHttpBridgeFactory(
        URLRequestContextGetter* getter) {
  return new browser_sync::TestHttpBridgeFactory;
}

void SyncBackendHostForProfileSyncTest::InitCore(
    const Core::DoInitializeOptions& options) {
  std::wstring user = L"testuser";
  core_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(core_.get(),
                        &SyncBackendHost::Core::DoInitializeForTest,
                        user,
                        options.http_bridge_factory,
                        options.delete_sync_data_folder));

  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_init_) {
    // The SyncBackend posts a task to the current loop when
    // initialization completes.
    MessageLoop::current()->Run();
  }
}

JsBackend* SyncBackendHostForProfileSyncTest::GetJsBackend() {
  // Return a non-NULL result only when the overridden function does.
  if (SyncBackendHost::GetJsBackend()) {
    return this;
  } else {
    NOTREACHED();
    return NULL;
  }
}

void SyncBackendHostForProfileSyncTest::SetParentJsEventRouter(
    JsEventRouter* router) {
  core_->SetParentJsEventRouter(router);
}

void SyncBackendHostForProfileSyncTest::RemoveParentJsEventRouter() {
  core_->RemoveParentJsEventRouter();
}

const JsEventRouter*
    SyncBackendHostForProfileSyncTest::GetParentJsEventRouter() const {
  return core_->GetParentJsEventRouter();
}

void SyncBackendHostForProfileSyncTest::ProcessMessage(
    const std::string& name, const JsArgList& args,
    const JsEventHandler* sender) {
  if (name.find("delay") != name.npos) {
    core_->RouteJsEvent(name, args, sender);
  } else {
    core_->RouteJsEventOnFrontendLoop(name, args, sender);
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
    ProfileSyncFactory* factory,
                       Profile* profile,
                       const std::string& test_user,
                       bool synchronous_backend_initialization,
                       Task* initial_condition_setup_task)
    : ProfileSyncService(factory, profile, test_user),
      synchronous_backend_initialization_(
          synchronous_backend_initialization),
      synchronous_sync_configuration_(false),
      num_expected_resumes_(1),
      num_expected_pauses_(1),
      initial_condition_setup_task_(initial_condition_setup_task),
      set_initial_sync_ended_on_init_(true) {
  RegisterPreferences();
  SetSyncSetupCompleted();
}

TestProfileSyncService::~TestProfileSyncService() {}

void TestProfileSyncService::SetInitialSyncEndedForEnabledTypes() {
  UserShare* user_share = GetUserShare();
  DirectoryManager* dir_manager = user_share->dir_manager.get();

  ScopedDirLookup dir(dir_manager, user_share->name);
  if (!dir.good())
    FAIL();

  ModelSafeRoutingInfo enabled_types;
  backend_->GetModelSafeRoutingInfo(&enabled_types);
  for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
       i != enabled_types.end(); ++i) {
    dir->set_initial_sync_ended_for_type(i->first, true);
  }
}

void TestProfileSyncService::OnBackendInitialized() {
  // Set this so below code can access GetUserShare().
  backend_initialized_ = true;

  // Set up any nodes the test wants around before model association.
  if (initial_condition_setup_task_) {
    initial_condition_setup_task_->Run();
    initial_condition_setup_task_ = NULL;
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
    }

    SetInitialSyncEndedForEnabledTypes();
  }

  ProfileSyncService::OnBackendInitialized();

  // TODO(akalin): Figure out a better way to do this.
  if (synchronous_backend_initialization_) {
    MessageLoop::current()->Quit();
  }
}

void TestProfileSyncService::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  ProfileSyncService::Observe(type, source, details);
  if (type == NotificationType::SYNC_CONFIGURE_DONE &&
      !synchronous_sync_configuration_) {
    MessageLoop::current()->Quit();
  }
}

void TestProfileSyncService::set_num_expected_resumes(int times) {
  num_expected_resumes_ = times;
}
void TestProfileSyncService::set_num_expected_pauses(int num) {
  num_expected_pauses_ = num;
}
void TestProfileSyncService::dont_set_initial_sync_ended_on_init() {
  set_initial_sync_ended_on_init_ = false;
}
void TestProfileSyncService::set_synchronous_sync_configuration() {
  synchronous_sync_configuration_ = true;
}

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
      profile(),
      num_expected_resumes_, num_expected_pauses_,
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_));
}

std::string TestProfileSyncService::GetLsidForAuthBootstraping() {
  return "foo";
}
