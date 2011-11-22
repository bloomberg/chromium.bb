// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_update_engine_client.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Invoke;
using chromeos::UpdateEngineClient;

static void RequestUpdateCheckSuccess(
    UpdateEngineClient::UpdateCheckCallback callback) {
  callback.Run(UpdateEngineClient::UPDATE_RESULT_SUCCESS);
}

class UpdateScreenTest : public WizardInProcessBrowserTest {
 public:
  UpdateScreenTest() : WizardInProcessBrowserTest("update"),
                       mock_update_engine_client_(NULL),
                       mock_network_library_(NULL) {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    MockSessionManagerClient* mock_session_manager_client
        = mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, EmitLoginPromptReady())
        .Times(1);

    mock_update_engine_client_
        = mock_dbus_thread_manager->mock_update_engine_client();

    // UpdateScreen::StartUpdate() will be called by the WizardController
    // just after creating the update screen, so the expectations for that
    // should be set up here.
    EXPECT_CALL(*mock_update_engine_client_, AddObserver(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_update_engine_client_, RemoveObserver(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_update_engine_client_, RequestUpdateCheck(_))
        .Times(1)
        .WillOnce(Invoke(RequestUpdateCheckSuccess));

    mock_network_library_ = cros_mock_->mock_network_library();
    EXPECT_CALL(*mock_network_library_, Connected())
        .Times(1)  // also called by NetworkMenu::InitMenuItems()
        .WillRepeatedly((Return(false)))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_network_library_, FindWifiDevice())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, FindEthernetDevice())
        .Times(AnyNumber());
  }

  virtual void SetUpOnMainThread() {
    mock_screen_observer_.reset(new MockScreenObserver());
    ASSERT_TRUE(controller() != NULL);
    update_screen_ = controller()->GetUpdateScreen();
    ASSERT_TRUE(update_screen_ != NULL);
    ASSERT_EQ(controller()->current_screen(), update_screen_);
    update_screen_->screen_observer_ = mock_screen_observer_.get();
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    update_screen_->screen_observer_ = (controller());
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  MockUpdateEngineClient* mock_update_engine_client_;
  MockNetworkLibrary* mock_network_library_;

  scoped_ptr<MockScreenObserver> mock_screen_observer_;
  UpdateScreen* update_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateScreenTest);
};

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestBasic) {
  ASSERT_TRUE(update_screen_->actor_ != NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestNoUpdate) {
  update_screen_->SetIgnoreIdleStatus(true);
  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_IDLE;
  update_screen_->UpdateStatusChanged(status);
  status.status = UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE;
  update_screen_->UpdateStatusChanged(status);
  status.status = UpdateEngineClient::UPDATE_STATUS_IDLE;
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(status));
  EXPECT_CALL(*mock_screen_observer_, OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateAvailable) {
  update_screen_->is_ignore_update_deadlines_ = true;

  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 0.0;
  update_screen_->UpdateStatusChanged(status);

  status.download_progress = 0.5;
  update_screen_->UpdateStatusChanged(status);

  status.download_progress = 1.0;
  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_VERIFYING;
  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_FINALIZING;
  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  EXPECT_CALL(*mock_update_engine_client_, RebootAfterUpdate())
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

static void RequestUpdateCheckFail(
    UpdateEngineClient::UpdateCheckCallback callback) {
  callback.Run(chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  // First, cancel the update that is already in progress.
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Run UpdateScreen::StartUpdate() again, but CheckForUpdate() will fail
  // issuing the update check this time.
  EXPECT_CALL(*mock_update_engine_client_, AddObserver(_))
      .Times(1);
  EXPECT_CALL(*mock_update_engine_client_, RemoveObserver(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_update_engine_client_, RequestUpdateCheck(_))
      .Times(1)
      .WillOnce(Invoke(RequestUpdateCheckFail));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen_->StartUpdate();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(status));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorUpdating) {
  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(status));
  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(status));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_UPDATING))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

}  // namespace chromeos
