// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/net/network_portal_detector_stub.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

const char kDefaultEthernetServicePath[] = "eth0";
const char kDefaultWifiServicePath[] = "wlan0";

static void RequestUpdateCheckSuccess(
    UpdateEngineClient::UpdateCheckCallback callback) {
  callback.Run(UpdateEngineClient::UPDATE_RESULT_SUCCESS);
}

}  // namespace

class UpdateScreenTest : public WizardInProcessBrowserTest {
 public:
  UpdateScreenTest() : WizardInProcessBrowserTest("update"),
                       mock_update_engine_client_(NULL),
                       mock_network_library_(NULL),
                       network_portal_detector_stub_(NULL) {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
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
    stub_ethernet_.reset(new EthernetNetwork(kDefaultEthernetServicePath));
    stub_wifi_.reset(new WifiNetwork(kDefaultWifiServicePath));
    EXPECT_CALL(*mock_network_library_, SetDefaultCheckPortalList())
        .Times(1);
    EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_network_library_, AddUserActionObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, FindWifiDevice())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, FindEthernetDevice())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, LoadOncNetworks(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_,
                FindNetworkByPath(kDefaultEthernetServicePath))
        .Times(AnyNumber())
        .WillRepeatedly((Return(stub_ethernet_.get())));
    EXPECT_CALL(*mock_network_library_,
                FindNetworkByPath(kDefaultWifiServicePath))
        .Times(AnyNumber())
        .WillRepeatedly((Return(stub_wifi_.get())));

    // Setup network portal detector to return online state for both
    // ethernet and wifi networks. Ethernet is an active network by
    // default.
    network_portal_detector_stub_ =
        static_cast<NetworkPortalDetectorStub*>(
            NetworkPortalDetector::GetInstance());
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    SetActiveNetwork(stub_ethernet_.get());
    SetDetectionResults(stub_ethernet_.get(), online_state);
    SetDetectionResults(stub_wifi_.get(), online_state);
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

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  void SetActiveNetwork(const Network* network) {
    DCHECK(network_portal_detector_stub_);
    network_portal_detector_stub_->SetActiveNetworkForTesting(network);
  }

  void SetDetectionResults(
      const Network* network,
      const NetworkPortalDetector::CaptivePortalState& state) {
    DCHECK(network_portal_detector_stub_);
    network_portal_detector_stub_->SetDetectionResultsForTesting(network,
                                                                 state);
  }

  void NotifyPortalDetectionCompleted() {
    DCHECK(network_portal_detector_stub_);
    network_portal_detector_stub_->NotifyObserversForTesting();
  }

  MockUpdateEngineClient* mock_update_engine_client_;
  MockNetworkLibrary* mock_network_library_;
  scoped_ptr<Network> stub_ethernet_;
  scoped_ptr<Network> stub_wifi_;
  scoped_ptr<MockScreenObserver> mock_screen_observer_;
  scoped_ptr<MockErrorScreenActor> mock_error_screen_actor_;
  scoped_ptr<MockErrorScreen> mock_error_screen_;
  UpdateScreen* update_screen_;
  NetworkPortalDetectorStub* network_portal_detector_stub_;

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
  update_screen_->StartNetworkCheck();
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

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTemproraryOfflineNetwork) {
  EXPECT_CALL(*mock_screen_observer_,
              OnExit(ScreenObserver::UPDATE_NOUPDATE))
      .Times(1);
  update_screen_->CancelUpdate();

  // Change ethernet state to portal.
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  portal_state.response_code = 200;
  SetDetectionResults(stub_ethernet_.get(), portal_state);

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
  SetDetectionResults(stub_ethernet_.get(), online_state);

  // Second notification from portal detector will be about online state,
  // so update screen will hide error message and proceed to update.
  EXPECT_CALL(*mock_screen_observer_, HideErrorScreen(update_screen_))
      .Times(1);
  EXPECT_CALL(*mock_update_engine_client_, RequestUpdateCheck(_))
      .Times(1)
      .WillOnce(Invoke(RequestUpdateCheckFail));
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
  SetDetectionResults(stub_ethernet_.get(), portal_state);

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
  SetActiveNetwork(stub_wifi_.get());
  SetDetectionResults(stub_wifi_.get(), proxy_state);

  // Update screen will show message about proxy error because wifie
  // network requires proxy authentication.
  EXPECT_CALL(*mock_error_screen_actor_,
              SetErrorState(ErrorScreen::ERROR_STATE_PROXY, std::string()))
      .Times(1);

  NotifyPortalDetectionCompleted();
}

}  // namespace chromeos
