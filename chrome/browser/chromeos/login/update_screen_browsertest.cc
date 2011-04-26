// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_update_library.h"
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
                       mock_login_library_(NULL),
                       mock_update_library_(NULL),
                       mock_network_library_(NULL) {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
    ASSERT_TRUE(CrosLibrary::Get()->EnsureLoaded());

    mock_login_library_ = new MockLoginLibrary();
    cros_mock_->test_api()->SetLoginLibrary(mock_login_library_, true);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);

    mock_update_library_ = new MockUpdateLibrary();
    cros_mock_->test_api()->SetUpdateLibrary(mock_update_library_, true);

    // UpdateScreen::StartUpdate() will be called by the WizardController
    // just after creating the update screen, so the expectations for that
    // should be set up here.
    EXPECT_CALL(*mock_update_library_, AddObserver(_))
        .Times(1);
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

  virtual void TearDownInProcessBrowserTestFixture() {
    cros_mock_->test_api()->SetUpdateLibrary(NULL, true);
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  MockLoginLibrary* mock_login_library_;
  MockUpdateLibrary* mock_update_library_;
  MockNetworkLibrary* mock_network_library_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateScreenTest);
};

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestBasic) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);
  UpdateView* update_view = update_screen->view();
  ASSERT_TRUE(update_view != NULL);
  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestNoUpdate) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);

  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_IDLE;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer, OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen->UpdateStatusChanged(mock_update_library_);

  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateAvailable) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);
  update_screen->SetAllUpdatesCritical(true);

  UpdateLibrary::Status status;

  status.status = UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.status = UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 0.0;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.download_progress = 0.5;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.download_progress = 1.0;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.status = UPDATE_STATUS_VERIFYING;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.status = UPDATE_STATUS_FINALIZING;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.status = UPDATE_STATUS_UPDATED_NEED_REBOOT;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_update_library_, RebootAfterUpdate())
      .Times(1);
  update_screen->UpdateStatusChanged(mock_update_library_);

  controller()->set_observer(NULL);
}

static void RequestUpdateCheckFail(UpdateCallback callback, void* userdata) {
  callback(userdata, chromeos::UPDATE_RESULT_FAILED, NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);

  // First, cancel the update that is already in progress.
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen->CancelUpdate();

  // Run UpdateScreen::StartUpdate() again, but CheckForUpdate() will fail
  // issuing the update check this time.
  EXPECT_CALL(*mock_update_library_, AddObserver(_))
      .Times(1);
  EXPECT_CALL(*mock_update_library_, RemoveObserver(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_update_library_, RequestUpdateCheck(_,_))
      .Times(1)
      .WillOnce(Invoke(RequestUpdateCheckFail));
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen->StartUpdate();

  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);

  UpdateLibrary::Status status;
  status.status = UPDATE_STATUS_ERROR;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen->UpdateStatusChanged(mock_update_library_);

  controller()->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorUpdating) {
  ASSERT_TRUE(controller() != NULL);
  scoped_ptr<MockScreenObserver> mock_screen_observer(new MockScreenObserver());
  controller()->set_observer(mock_screen_observer.get());
  UpdateScreen* update_screen = controller()->GetUpdateScreen();
  ASSERT_TRUE(update_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), update_screen);

  UpdateLibrary::Status status;

  status.status = UPDATE_STATUS_UPDATE_AVAILABLE;
  status.new_version = "latest and greatest";
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  update_screen->UpdateStatusChanged(mock_update_library_);

  status.status = UPDATE_STATUS_ERROR;
  EXPECT_CALL(*mock_update_library_, status())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(status));
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::UPDATE_ERROR_UPDATING))
      .Times(1);
  update_screen->UpdateStatusChanged(mock_update_library_);

  controller()->set_observer(NULL);
}

}  // namespace chromeos
