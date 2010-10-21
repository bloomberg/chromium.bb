// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    TestProfileSyncService* service,
    Profile* profile,
    const FilePath& profile_path,
    const DataTypeController::TypeMap& data_type_controllers,
    Task* initial_condition_setup_task,
    int num_expected_resumes,
    int num_expected_pauses,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init)
    : browser_sync::SyncBackendHost(service, profile, profile_path,
                                    data_type_controllers),
      initial_condition_setup_task_(initial_condition_setup_task),
      set_initial_sync_ended_on_init_(set_initial_sync_ended_on_init),
      synchronous_init_(synchronous_init),
      test_service_(service) {
  // By default, the RequestPause and RequestResume methods will
  // send the confirmation notification and return true.
  ON_CALL(*this, RequestPause()).
      WillByDefault(testing::DoAll(CallOnPaused(core_),
                                   testing::Return(true)));
  ON_CALL(*this, RequestResume()).
      WillByDefault(testing::DoAll(CallOnResumed(core_),
                                   testing::Return(true)));
  ON_CALL(*this, RequestNudge()).WillByDefault(testing::Invoke(this,
       &SyncBackendHostForProfileSyncTest::
           SimulateSyncCycleCompletedInitialSyncEnded));

  EXPECT_CALL(*this, RequestPause()).Times(num_expected_pauses);
  EXPECT_CALL(*this, RequestResume()).Times(num_expected_resumes);
  EXPECT_CALL(*this, RequestNudge()).
      Times(set_initial_sync_ended_on_init ? 0 : 1);
}

void SyncBackendHostForProfileSyncTest::SetInitialSyncEndedForEnabledTypes() {
  UserShare* user_share = core_->syncapi()->GetUserShare();
  DirectoryManager* dir_manager = user_share->dir_manager.get();

  ScopedDirLookup dir(dir_manager, user_share->name);
  if (!dir.good())
    FAIL();

  ModelSafeRoutingInfo enabled_types;
  GetModelSafeRoutingInfo(&enabled_types);
  for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
      i != enabled_types.end(); ++i) {
    dir->set_initial_sync_ended_for_type(i->first, true);
  }
}

void SyncBackendHostForProfileSyncTest::
    HandleInitializationCompletedOnFrontendLoop() {
  set_syncapi_initialized();  // Need to do this asap so task below works.

  // Set up any nodes the test wants around before model association.
  if (initial_condition_setup_task_) {
    initial_condition_setup_task_->Run();
  }

  // Pretend we downloaded initial updates and set initial sync ended bits
  // if we were asked to.
  if (set_initial_sync_ended_on_init_) {
    UserShare* user_share = core_->syncapi()->GetUserShare();
    DirectoryManager* dir_manager = user_share->dir_manager.get();

    ScopedDirLookup dir(dir_manager, user_share->name);
    if (!dir.good())
      FAIL();

    // Do this check as some tests load from storage and we don't want to add
    // twice.
    // TODO(tim): This is madness.  Too much test setup code!
    if (!dir->initial_sync_ended_for_type(syncable::NIGORI)) {
      ProfileSyncServiceTestHelper::CreateRoot(
          syncable::NIGORI, test_service_, test_service_->id_factory());
    }

    SetInitialSyncEndedForEnabledTypes();
  }

  SyncBackendHost::HandleInitializationCompletedOnFrontendLoop();
}

// Called when a nudge comes in.
void SyncBackendHostForProfileSyncTest::
    SimulateSyncCycleCompletedInitialSyncEnded() {
  syncable::ModelTypeBitSet sync_ended;
  ModelSafeRoutingInfo enabled_types;
  GetModelSafeRoutingInfo(&enabled_types);
  for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
      i != enabled_types.end(); ++i) {
    sync_ended.set(i->first);
  }
  core_->HandleSyncCycleCompletedOnFrontendLoop(new SyncSessionSnapshot(
      SyncerStatus(), ErrorCounters(), 0, 0, false,
      sync_ended, false, false, 0, 0, false));
}

void SyncBackendHostForProfileSyncTest::InitCore(
    const Core::DoInitializeOptions& options) {
  std::wstring user = L"testuser";
  core_loop()->PostTask(FROM_HERE,
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

void SyncBackendHostForProfileSyncTest::
    SetDefaultExpectationsForWorkerCreation(ProfileMock* profile) {
  EXPECT_CALL(*profile, GetPasswordStore(testing::_)).
    WillOnce(testing::Return((PasswordStore*)NULL));
  EXPECT_CALL(*profile, GetHistoryService(testing::_)).
    WillOnce(testing::Return((HistoryService*)NULL));
}

}  // namespace browser_sync

TestProfileSyncService::TestProfileSyncService(ProfileSyncFactory* factory,
    Profile* profile,
    const std::string& test_user,
    bool synchronous_backend_initialization,
    Task* initial_condition_setup_task)
    : ProfileSyncService(factory, profile,
                         !test_user.empty() ?
                         test_user : ""),
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

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
    this, profile(),
    profile()->GetPath(), data_type_controllers(),
    initial_condition_setup_task_.release(),
    num_expected_resumes_, num_expected_pauses_,
    set_initial_sync_ended_on_init_,
    synchronous_backend_initialization_));
}

void TestProfileSyncService::OnBackendInitialized() {
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
