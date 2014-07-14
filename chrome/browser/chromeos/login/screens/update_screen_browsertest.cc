// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

namespace {

const char kStubEthernetGuid[] = "eth0";
const char kStubWifiGuid[] = "wlan0";

}  // namespace

class UpdateScreenTest : public WizardInProcessBrowserTest {
 public:
  UpdateScreenTest() : WizardInProcessBrowserTest("update"),
                       fake_update_engine_client_(NULL),
                       network_portal_detector_(NULL) {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    FakeDBusThreadManager* fake_dbus_thread_manager =
        new FakeDBusThreadManager;
    fake_dbus_thread_manager->SetFakeClients();
    fake_update_engine_client_ = new FakeUpdateEngineClient;
    fake_dbus_thread_manager->SetUpdateEngineClient(
        scoped_ptr<UpdateEngineClient>(fake_update_engine_client_));

    DBusThreadManager::SetInstanceForTesting(fake_dbus_thread_manager);
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Setup network portal detector to return online state for both
    // ethernet and wifi networks. Ethernet is an active network by
    // default.
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(network_portal_detector_);
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    SetDefaultNetwork(kStubEthernetGuid);
    SetDetectionResults(kStubEthernetGuid, online_state);
    SetDetectionResults(kStubWifiGuid, online_state);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WizardInProcessBrowserTest::SetUpOnMainThread();

    mock_screen_observer_.reset(new MockScreenObserver());
    mock_error_screen_actor_.reset(new MockErrorScreenActor());
    mock_error_screen_.reset(
        new MockErrorScreen(mock_screen_observer_.get(),
                            mock_error_screen_actor_.get()));
    EXPECT_CALL(*mock_screen_observer_, ShowCurrentScreen())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_screen_observer_, GetErrorScreen())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_error_screen_.get()));

    ASSERT_TRUE(WizardController::default_controller() != NULL);
    update_screen_ = WizardController::default_controller()->GetUpdateScreen();
    ASSERT_TRUE(update_screen_ != NULL);
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              update_screen_);
    update_screen_->screen_observer_ = mock_screen_observer_.get();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    mock_error_screen_.reset();
    mock_error_screen_actor_.reset();
    WizardInProcessBrowserTest::CleanUpOnMainThread();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    NetworkPortalDetector::Shutdown();
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetDefaultNetwork(const std::string& guid) {
    DCHECK(network_portal_detector_);
    network_portal_detector_->SetDefaultNetworkForTesting(guid);
  }

  void SetDetectionResults(
      const std::string& guid,
      const NetworkPortalDetector::CaptivePortalState& state) {
    DCHECK(network_portal_detector_);
    network_portal_detector_->SetDetectionResultsForTesting(guid, state);
  }

  void NotifyPortalDetectionCompleted() {
    DCHECK(network_portal_detector_);
    network_portal_detector_->NotifyObserversForTesting();
  }

  FakeUpdateEngineClient* fake_update_engine_client_;
  scoped_ptr<MockScreenObserver> mock_screen_observer_;
  scoped_ptr<MockErrorScreenActor> mock_error_screen_actor_;
  scoped_ptr<MockErrorScreen> mock_error_screen_;
  UpdateScreen* update_screen_;
  NetworkPortalDetectorTestImpl* network_portal_detector_;

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
  fake_update_engine_client_->set_default_status(status);

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
  update_screen_->UpdateStatusChanged(status);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, fake_update_engine_client_->reboot_after_update_call_count());
  // Check that OOBE will resume back at this screen.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(update_screen_->GetName(),
      g_browser_process->local_state()->GetString(prefs::kOobeScreenPending));
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  // First, cancel the update that is already in progress.
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  fake_update_engine_client_->set_update_check_result(
      chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);
  update_screen_->StartNetworkCheck();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  fake_update_engine_client_->set_default_status(status);

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
  fake_update_engine_client_->set_default_status(status);

  update_screen_->UpdateStatusChanged(status);

  status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  fake_update_engine_client_->set_default_status(status);

  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_UPDATING))
      .Times(1);
  update_screen_->UpdateStatusChanged(status);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTemproraryOfflineNetwork) {
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Change ethernet state to portal.
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  portal_state.response_code = 200;
  SetDetectionResults(kStubEthernetGuid, portal_state);

  // Update screen will show error message about portal state because
  // ethernet is behind captive portal.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetUIState(ErrorScreen::UI_STATE_UPDATE))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_PORTAL, std::string()))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_, FixCaptivePortal())
      .Times(1);
  EXPECT_CALL(*mock_screen_observer_, ShowErrorScreen())
      .Times(1);

  update_screen_->StartNetworkCheck();

  NetworkPortalDetector::CaptivePortalState online_state;
  online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  online_state.response_code = 204;
  SetDetectionResults(kStubEthernetGuid, online_state);

  // Second notification from portal detector will be about online state,
  // so update screen will hide error message and proceed to update.
  EXPECT_CALL(*mock_screen_observer_, HideErrorScreen(update_screen_))
      .Times(1);
  fake_update_engine_client_->set_update_check_result(
      chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);

  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);

  NotifyPortalDetectionCompleted();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTwoOfflineNetworks) {
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Change ethernet state to portal.
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  portal_state.response_code = 200;
  SetDetectionResults(kStubEthernetGuid, portal_state);

  // Update screen will show error message about portal state because
  // ethernet is behind captive portal.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetUIState(ErrorScreen::UI_STATE_UPDATE))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_PORTAL, std::string()))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_, FixCaptivePortal())
      .Times(1);
  EXPECT_CALL(*mock_screen_observer_, ShowErrorScreen())
      .Times(1);

  update_screen_->StartNetworkCheck();

  // Change active network to the wifi behind proxy.
  NetworkPortalDetector::CaptivePortalState proxy_state;
  proxy_state.status =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
  proxy_state.response_code = -1;
  SetDefaultNetwork(kStubWifiGuid);
  SetDetectionResults(kStubWifiGuid, proxy_state);

  // Update screen will show message about proxy error because wifie
  // network requires proxy authentication.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_PROXY, std::string()))
      .Times(1);

  NotifyPortalDetectionCompleted();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestVoidNetwork) {
  SetDefaultNetwork(std::string());

  // Cancels pending update request.
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // First portal detection attempt returns NULL network and undefined
  // results, so detection is restarted.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetUIState(_))
      .Times(Exactly(0));
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(_, _))
      .Times(Exactly(0));
  EXPECT_CALL(*mock_screen_observer_, ShowErrorScreen())
      .Times(Exactly(0));
  update_screen_->StartNetworkCheck();

  // Second portal detection also returns NULL network and undefined
  // results.  In this case, offline message should be displayed.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetUIState(ErrorScreen::UI_STATE_UPDATE))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_OFFLINE, std::string()))
      .Times(1);
  EXPECT_CALL(*mock_screen_observer_, ShowErrorScreen())
      .Times(1);
  base::MessageLoop::current()->RunUntilIdle();
  NotifyPortalDetectionCompleted();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestAPReselection) {
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Change ethernet state to portal.
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  portal_state.response_code = 200;
  SetDetectionResults(kStubEthernetGuid, portal_state);

  // Update screen will show error message about portal state because
  // ethernet is behind captive portal.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetUIState(ErrorScreen::UI_STATE_UPDATE))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_PORTAL, std::string()))
      .Times(1);
  EXPECT_CALL(*mock_error_screen_actor_, FixCaptivePortal())
      .Times(1);
  EXPECT_CALL(*mock_screen_observer_, ShowErrorScreen())
      .Times(1);

  update_screen_->StartNetworkCheck();

  // User re-selects the same network manually. In this case, hide
  // offline message and skip network check. Since ethernet is still
  // behind portal, update engine fails to update.
  EXPECT_CALL(*mock_screen_observer_, HideErrorScreen(update_screen_))
      .Times(1);
  fake_update_engine_client_->set_update_check_result(
      chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_ERROR_CHECKING_FOR_UPDATE))
      .Times(1);

  update_screen_->OnConnectToNetworkRequested();
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace chromeos
