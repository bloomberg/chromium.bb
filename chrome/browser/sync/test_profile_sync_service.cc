// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test_profile_sync_service.h"

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

namespace browser_sync {

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    TestProfileSyncService* service,
    Profile* profile,
    Task* initial_condition_setup_task,
    int num_expected_resumes,
    int num_expected_pauses,
    bool set_initial_sync_ended_on_init,
    bool synchronous_init)
    : browser_sync::SyncBackendHost(service, profile),
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
  ON_CALL(*this, RequestNudge()).WillByDefault(
      testing::Invoke(this,
                      &SyncBackendHostForProfileSyncTest::
                      SimulateSyncCycleCompletedInitialSyncEnded));

  EXPECT_CALL(*this, RequestPause()).Times(num_expected_pauses);
  EXPECT_CALL(*this, RequestResume()).Times(num_expected_resumes);
  EXPECT_CALL(*this,
              RequestNudge()).Times(set_initial_sync_ended_on_init ? 0 : 1);
}

void
SyncBackendHostForProfileSyncTest::
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

    if (!dir->initial_sync_ended_for_type(syncable::NIGORI)) {
      ProfileSyncServiceTestHelper::CreateRoot(
          syncable::NIGORI, test_service_, test_service_->id_factory());
    }

    SetInitialSyncEndedForEnabledTypes();
  }

  SyncBackendHost::HandleInitializationCompletedOnFrontendLoop();
}

}  // namespace browser_sync

void TestProfileSyncService::CreateBackend() {
  backend_.reset(new browser_sync::SyncBackendHostForProfileSyncTest(
      this, profile(),
      initial_condition_setup_task_.release(),
      num_expected_resumes_, num_expected_pauses_,
      set_initial_sync_ended_on_init_,
      synchronous_backend_initialization_));
}
