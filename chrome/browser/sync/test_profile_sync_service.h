// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#pragma once

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/test_http_bridge_factory.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class TestProfileSyncService;

using browser_sync::ModelSafeRoutingInfo;
using browser_sync::sessions::ErrorCounters;
using browser_sync::sessions::SyncerStatus;
using browser_sync::sessions::SyncSessionSnapshot;
using sync_api::UserShare;
using syncable::DirectoryManager;
using syncable::ModelType;
using syncable::ScopedDirLookup;

ACTION_P(CallOnPaused, core) {
  core->OnPaused();
};

ACTION_P(CallOnResumed, core) {
  core->OnResumed();
}

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0, arg1);
}

namespace browser_sync {

// Mocks out the SyncerThread operations (Pause/Resume) since no thread is
// running in these tests, and allows tests to provide a task on construction
// to set up initial nodes to mock out an actual server initial sync
// download.
class SyncBackendHostForProfileSyncTest : public SyncBackendHost {
 public:
  // |initial_condition_setup_task| can be used to populate nodes before the
  //     OnBackendInitialized callback fires.
  // |set_initial_sync_ended_on_init| determines whether we pretend that a full
  //     initial download has occurred and set bits for enabled data types.  If
  //     this is false, configuring data types will require a syncer nudge.
  // |synchronous_init| causes initialization to block until the syncapi has
  //     completed setting itself up and called us back.
  SyncBackendHostForProfileSyncTest(
      TestProfileSyncService* service,
      Profile* profile,
      Task* initial_condition_setup_task,
      int num_expected_resumes,
      int num_expected_pauses,
      bool set_initial_sync_ended_on_init,
      bool synchronous_init);

  MOCK_METHOD0(RequestPause, bool());
  MOCK_METHOD0(RequestResume, bool());
  MOCK_METHOD0(RequestNudge, void());

  void SetInitialSyncEndedForEnabledTypes() {
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

  virtual void ConfigureDataTypes(
      const DataTypeController::TypeMap& data_type_controllers,
      const syncable::ModelTypeSet& types,
      CancelableTask* ready_task) {
    SetAutofillMigrationState(syncable::MIGRATED);
    SyncBackendHost::ConfigureDataTypes(
        data_type_controllers, types, ready_task);
  }

  // Called when a nudge comes in.
  void SimulateSyncCycleCompletedInitialSyncEnded() {
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
        sync_ended, download_progress_markers, false, false, 0, 0, false));
  }

  virtual void HandleInitializationCompletedOnFrontendLoop();

  virtual sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
      URLRequestContextGetter* getter) {
    return new browser_sync::TestHttpBridgeFactory;
  }

  virtual void InitCore(const Core::DoInitializeOptions& options) {
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

  static void SetDefaultExpectationsForWorkerCreation(ProfileMock* profile) {
    EXPECT_CALL(*profile, GetPasswordStore(testing::_)).
        WillOnce(testing::Return((PasswordStore*)NULL));
  }

  static void SetHistoryServiceExpectations(ProfileMock* profile) {
    EXPECT_CALL(*profile, GetHistoryService(testing::_)).
        WillOnce(testing::Return((HistoryService*)NULL));
  }

 private:
  Task* initial_condition_setup_task_;
  bool set_initial_sync_ended_on_init_;
  bool synchronous_init_;
  TestProfileSyncService* test_service_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  TestProfileSyncService(ProfileSyncFactory* factory,
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
  virtual ~TestProfileSyncService() { }

  virtual void OnBackendInitialized() {
    ProfileSyncService::OnBackendInitialized();
    // TODO(akalin): Figure out a better way to do this.
    if (synchronous_backend_initialization_) {
      MessageLoop::current()->Quit();
    }
  }

  virtual void Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
    ProfileSyncService::Observe(type, source, details);
    if (type == NotificationType::SYNC_CONFIGURE_DONE &&
        !synchronous_sync_configuration_) {
      MessageLoop::current()->Quit();
    }
  }

  void set_num_expected_resumes(int times) {
    num_expected_resumes_ = times;
  }
  void set_num_expected_pauses(int num) {
    num_expected_pauses_ = num;
  }
  void dont_set_initial_sync_ended_on_init() {
    set_initial_sync_ended_on_init_ = false;
  }
  void set_synchronous_sync_configuration() {
    synchronous_sync_configuration_ = true;
  }

  browser_sync::TestIdFactory* id_factory() { return &id_factory_; }

 protected:
  virtual void CreateBackend();

 private:
  // When testing under ChromiumOS, this method must not return an empty
  // value value in order for the profile sync service to start.
  virtual std::string GetLsidForAuthBootstraping() {
    return "foo";
  }

  bool synchronous_backend_initialization_;

  // Set to true when a mock data type manager is being used and the configure
  // step is performed synchronously.
  bool synchronous_sync_configuration_;
  bool set_expect_resume_expectations_;
  int num_expected_resumes_;
  int num_expected_pauses_;

  scoped_ptr<Task> initial_condition_setup_task_;
  bool set_initial_sync_ended_on_init_;
  browser_sync::TestIdFactory id_factory_;
};



#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
