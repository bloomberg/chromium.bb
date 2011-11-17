// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_update_library.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
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

static void RequestUpdateCheckSuccess(UpdateCallback callback, void* userdata) {
  callback(userdata, chromeos::UPDATE_RESULT_SUCCESS, NULL);
}

class UpdateScreenTest : public WizardInProcessBrowserTest {
 public:
  UpdateScreenTest() : WizardInProcessBrowserTest("update"),
                       mock_update_library_(NULL),
                       mock_network_library_(NULL) {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
    ASSERT_TRUE(CrosLibrary::Get()->EnsureLoaded());

    MockSessionManagerClient* mock_session_manager_client
        = mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, EmitLoginPromptReady())
        .Times(1);

    mock_update_library_ = new MockUpdateLibrary();
    cros_mock_->test_api()->SetUpdateLibrary(mock_update_library_, true);

    // UpdateScreen::StartUpdate() will be called by the WizardController
    // just after creating the update screen, so the expectations for that
    // should be set up here.
    EXPECT_CALL(*mock_update_library_, AddObserver(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_update_library_, RemoveObserver(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_update_library_, RequestUpdateCheck(_,_))
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
    cros_mock_->test_api()->SetUpdateLibrary(NULL, true);
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  MockUpdateLibrary* mock_update_library_;
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
  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_IDLE;
  update_screen_->UpdateStatusChanged(status);
  status.status = UPDATE_STATUS_CHECKING_FOR_UPDATE;
  update_screen_->UpdateStatusChanged(status);
  status.status = UPDATE_STATUS_IDLE;
  // status() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer_, OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateAvailable) {
  update_screen_->is_ignore_update_deadlines_ = true;

  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  update_screen_->UpdateStatusChanged(status);

  status.status = UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 0.0;
  update_screen_->UpdateStatusChanged(status);

  status.download_progress = 0.5;
  update_screen_->UpdateStatusChanged(status);

  status.download_progress = 1.0;
  update_screen_->UpdateStatusChanged(status);

  status.status = UPDATE_STATUS_VERIFYING;
  update_screen_->UpdateStatusChanged(status);

  status.status = UPDATE_STATUS_FINALIZING;
  update_screen_->UpdateStatusChanged(status);

  status.status = UPDATE_STATUS_UPDATED_NEED_REBOOT;
  EXPECT_CALL(*mock_update_library_, RebootAfterUpdate())
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

static void RequestUpdateCheckFail(UpdateCallback callback, void* userdata) {
  callback(userdata, chromeos::UPDATE_RESULT_FAILED, NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  // First, cancel the update that is already in progress.
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Run UpdateScreen::StartUpdate() again, but CheckForUpdate() will fail
  // issuing the update check this time.
  EXPECT_CALL(*mock_update_library_, AddObserver(_))
      .Times(1);
  EXPECT_CALL(*mock_update_library_, RemoveObserver(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_update_library_, RequestUpdateCheck(_,_))
      .Times(1)
      .WillOnce(Invoke(RequestUpdateCheckFail));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen_->StartUpdate();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_ERROR;
  // status() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorUpdating) {
  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  // status() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen_->UpdateStatusChanged(status);

  status.status = UPDATE_STATUS_ERROR;
  // status() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_UPDATING))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

}  // namespace chromeos
