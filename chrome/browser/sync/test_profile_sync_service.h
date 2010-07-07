// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/sync/test_http_bridge_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

ACTION_P(CallOnPaused, core) {
  core->OnPaused();
};

ACTION_P(CallOnResumed, core) {
  core->OnResumed();
}

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0, arg1);
}

// Mocks out the SyncerThread operations (Pause/Resume) since no thread is
// running in these tests, and allows tests to provide a task on construction
// to set up initial nodes to mock out an actual server initial sync
// download.
class SyncBackendHostForProfileSyncTest :
    public browser_sync::SyncBackendHost {
 public:
  SyncBackendHostForProfileSyncTest(browser_sync::SyncFrontend* frontend,
      Profile* profile,
      const FilePath& profile_path,
      const browser_sync::DataTypeController::TypeMap& data_type_controllers,
      Task* initial_condition_setup_task,
      int num_expected_resumes,
      int num_expected_pauses)
      : browser_sync::SyncBackendHost(frontend, profile, profile_path,
                                      data_type_controllers),
        initial_condition_setup_task_(initial_condition_setup_task) {
    // By default, the RequestPause and RequestResume methods will
    // send the confirmation notification and return true.
    ON_CALL(*this, RequestPause()).
        WillByDefault(testing::DoAll(CallOnPaused(core_),
                                     testing::Return(true)));
    ON_CALL(*this, RequestResume()).
        WillByDefault(testing::DoAll(CallOnResumed(core_),
                                     testing::Return(true)));
    EXPECT_CALL(*this, RequestPause()).Times(num_expected_pauses);
    EXPECT_CALL(*this, RequestResume()).Times(num_expected_resumes);
  }

  MOCK_METHOD0(RequestPause, bool());
  MOCK_METHOD0(RequestResume, bool());

  virtual void HandleInitializationCompletedOnFrontendLoop() {
    if (initial_condition_setup_task_) {
      initial_condition_setup_task_->Run();
    }

    SyncBackendHost::HandleInitializationCompletedOnFrontendLoop();
  }
 private:
  Task* initial_condition_setup_task_;

};

class TestProfileSyncService : public ProfileSyncService {
 public:
  TestProfileSyncService(ProfileSyncFactory* factory,
                         Profile* profile,
                         bool bootstrap_sync_authentication,
                         bool synchronous_backend_initialization,
                         Task* initial_condition_setup_task)
      : ProfileSyncService(factory, profile, bootstrap_sync_authentication),
        synchronous_backend_initialization_(
            synchronous_backend_initialization),
        synchronous_sync_configuration_(false),
        num_expected_resumes_(1),
        num_expected_pauses_(1),
        initial_condition_setup_task_(initial_condition_setup_task) {
    RegisterPreferences();
    SetSyncSetupCompleted();
  }
  virtual ~TestProfileSyncService() { }

  virtual void InitializeBackend(bool delete_sync_data_folder) {
    browser_sync::TestHttpBridgeFactory* factory =
        new browser_sync::TestHttpBridgeFactory();
    browser_sync::TestHttpBridgeFactory* factory2 =
        new browser_sync::TestHttpBridgeFactory();
    backend_->Shutdown(false);
    backend_.reset(new SyncBackendHostForProfileSyncTest(this, profile(),
        profile()->GetPath(), data_type_controllers(),
        initial_condition_setup_task_.release(),
        num_expected_resumes_, num_expected_pauses_));
    backend()->InitializeForTestMode(L"testuser", factory, factory2,
        delete_sync_data_folder, browser_sync::kDefaultNotificationMethod);
    // TODO(akalin): Figure out a better way to do this.
    if (synchronous_backend_initialization_) {
      // The SyncBackend posts a task to the current loop when
      // initialization completes.
      MessageLoop::current()->Run();
      // Initialization is synchronous for test mode, so we should be
      // good to go.
      DCHECK(sync_initialized());
    }
  }

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

  void set_synchronous_sync_configuration() {
    synchronous_sync_configuration_ = true;
  }

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
};

#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
