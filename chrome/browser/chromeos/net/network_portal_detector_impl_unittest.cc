// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "chrome/browser/captive_portal/testing_utils.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

}  // namespace

// Service paths for stub network devices.
const char* kStubEthernet = "stub_ethernet";
const char* kStubWireless1 = "stub_wifi1";
const char* kStubWireless2 = "stub_wifi2";
const char* kStubCellular = "stub_cellular";

class NetworkPortalDetectorImplTest
    : public testing::Test,
      public captive_portal::CaptivePortalDetectorTestBase {
 protected:
  virtual void SetUp() {
    DBusThreadManager::InitializeWithStub();
    SetupNetworkHandler();

    profile_.reset(new TestingProfile());
    network_portal_detector_.reset(
        new NetworkPortalDetectorImpl(profile_->GetRequestContext()));
    network_portal_detector_->Init();
    network_portal_detector_->Enable(false);

    set_detector(network_portal_detector_->captive_portal_detector_.get());

    // Prevents flakiness due to message loop delays.
    set_time_ticks(base::TimeTicks::Now());
  }

  virtual void TearDown() {
    network_portal_detector_->Shutdown();
    profile_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void CheckPortalState(NetworkPortalDetector::CaptivePortalStatus status,
                        int response_code,
                        const std::string& network_service_path) {
    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->GetNetworkState(
            network_service_path);
    NetworkPortalDetector::CaptivePortalState state =
        network_portal_detector()->GetCaptivePortalState(network);
    ASSERT_EQ(status, state.status);
    ASSERT_EQ(response_code, state.response_code);
  }

  void CheckRequestTimeoutAndCompleteAttempt(
      int expected_attempt_count,
      int expected_request_timeout_sec,
      int net_error,
      int status_code) {
    ASSERT_TRUE(is_state_checking_for_portal());
    ASSERT_EQ(expected_attempt_count, attempt_count());
    ASSERT_EQ(expected_request_timeout_sec, get_request_timeout_sec());
    CompleteURLFetch(net_error, status_code, NULL);
  }

  Profile* profile() { return profile_.get(); }

  NetworkPortalDetectorImpl* network_portal_detector() {
    return network_portal_detector_.get();
  }

  NetworkPortalDetectorImpl::State state() {
    return network_portal_detector()->state();
  }

  bool start_detection_if_idle() {
    return network_portal_detector()->StartDetectionIfIdle();
  }

  void enable_lazy_detection() {
    network_portal_detector()->EnableLazyDetection();
  }

  void disable_lazy_detection() {
    network_portal_detector()->DisableLazyDetection();
  }

  void cancel_portal_detection() {
    network_portal_detector()->CancelPortalDetection();
  }

  bool detection_timeout_is_cancelled() {
    return
        network_portal_detector()->DetectionTimeoutIsCancelledForTesting();
  }

  int get_request_timeout_sec() {
    return network_portal_detector()->GetRequestTimeoutSec();
  }

  bool is_state_idle() {
    return (NetworkPortalDetectorImpl::STATE_IDLE == state());
  }

  bool is_state_portal_detection_pending() {
    return (NetworkPortalDetectorImpl::STATE_PORTAL_CHECK_PENDING == state());
  }

  bool is_state_checking_for_portal() {
    return (NetworkPortalDetectorImpl::STATE_CHECKING_FOR_PORTAL == state());
  }

  void set_request_timeout(const base::TimeDelta& timeout) {
    network_portal_detector()->set_request_timeout_for_testing(timeout);
  }

  const base::TimeDelta& next_attempt_delay() {
    return network_portal_detector()->next_attempt_delay_for_testing();
  }

  int attempt_count() {
    return network_portal_detector()->attempt_count_for_testing();
  }

  void set_attempt_count(int ac) {
    return network_portal_detector()->set_attempt_count_for_testing(ac);
  }

  void set_min_time_between_attempts(const base::TimeDelta& delta) {
    network_portal_detector()->set_min_time_between_attempts_for_testing(delta);
  }

  void set_lazy_check_interval(const base::TimeDelta& delta) {
    network_portal_detector()->set_lazy_check_interval_for_testing(delta);
  }

  void set_time_ticks(const base::TimeTicks& time_ticks) {
    network_portal_detector()->set_time_ticks_for_testing(time_ticks);
  }

  void SetBehindPortal(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(service_path),
        shill::kStateProperty, base::StringValue(shill::kStatePortal),
        base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkDeviceEnabled(const std::string& type, bool enabled) {
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::Primitive(type),
        enabled,
        network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetConnected(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->Connect(
        dbus::ObjectPath(service_path),
        base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

 private:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;
    const bool add_to_watchlist = true;
    service_test->AddService(kStubEthernet,
                             kStubEthernet,
                             shill::kTypeEthernet, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->AddService(kStubWireless1,
                             kStubWireless1,
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->AddService(kStubWireless2,
                             kStubWireless2,
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->AddService(kStubCellular,
                             kStubCellular,
                             shill::kTypeCellular, shill::kStateIdle,
                             add_to_visible, add_to_watchlist);
  }

  void SetupNetworkHandler() {
    SetupDefaultShillState();
    NetworkHandler::Initialize();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<NetworkPortalDetectorImpl> network_portal_detector_;
};

TEST_F(NetworkPortalDetectorImplTest, NoPortal) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);

  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, Portal) {
  ASSERT_TRUE(is_state_idle());

  // Check HTTP 200 response code.
  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);

  // Check HTTP 301 response code.
  SetConnected(kStubWireless2);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 301, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 301,
                   kStubWireless2);

  // Check HTTP 302 response code.
  SetConnected(kStubEthernet);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 302, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 302,
                   kStubEthernet);
}

TEST_F(NetworkPortalDetectorImplTest, TwoNetworks) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  // wifi is in portal state.
  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubEthernet);
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubEthernet);
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, NetworkChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);

  // WiFi is in portal state.
  fetcher()->set_response_code(200);
  ASSERT_TRUE(is_state_checking_for_portal());

  // Active network is changed during portal detection for wifi.
  SetConnected(kStubEthernet);

  // Portal detection for wifi is cancelled, portal detection for
  // ethernet is initiated.
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubEthernet);

  // As active network was changed during portal detection for wifi
  // network, it's state must be unknown.
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateNotChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_idle());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateChanged) {
  // Test for Portal -> Online -> Portal network state transitions.
  ASSERT_TRUE(is_state_idle());

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubWireless1);

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionTimeout) {
  ASSERT_TRUE(is_state_idle());

  // For instantaneous timeout.
  set_request_timeout(base::TimeDelta::FromSeconds(0));

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  base::RunLoop().RunUntilIdle();

  // First portal detection timeouts, next portal detection is
  // scheduled.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionRetryAfter) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 101\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 101 seconds.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(101), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectorRetryAfterIsSmall) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 1\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 3 seconds (due to minimum time between detection
  // attemps).
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorImplTest, FirstAttemptFailed) {
  ASSERT_TRUE(is_state_idle());

  set_min_time_between_attempts(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(2, attempt_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, AllAttemptsFailed) {
  ASSERT_TRUE(is_state_idle());

  set_min_time_between_attempts(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(2, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(3, attempt_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 503,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, ProxyAuthRequired) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED, 407,
      kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, NoResponseButBehindPortal) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, DisableLazyDetectionWhilePendingRequest) {
  ASSERT_TRUE(is_state_idle());
  set_attempt_count(3);
  enable_lazy_detection();
  ASSERT_TRUE(is_state_portal_detection_pending());
  disable_lazy_detection();

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(NetworkPortalDetectorImplTest, LazyDetectionForOnlineNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  set_lazy_check_interval(base::TimeDelta());

  SetConnected(kStubWireless1);
  enable_lazy_detection();
  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_lazy_detection();

  // One more detection result, because DizableLazyDetection() doesn't
  // cancel last detection request.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, LazyDetectionForPortalNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  set_lazy_check_interval(base::TimeDelta());

  SetConnected(kStubWireless1);
  enable_lazy_detection();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_lazy_detection();

  // One more detection result, because DizableLazyDetection() doesn't
  // cancel last detection request.
  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, DetectionTimeoutIsCancelled) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);

  cancel_portal_detection();

  ASSERT_TRUE(is_state_idle());
  ASSERT_TRUE(detection_timeout_is_cancelled());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   kStubWireless1);
}

TEST_F(NetworkPortalDetectorImplTest, TestDetectionRestart) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  // First portal detection attempts determines ONLINE state.
  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());
  ASSERT_FALSE(start_detection_if_idle());

  CompleteURLFetch(net::OK, 204, NULL);

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   kStubWireless1);
  ASSERT_TRUE(is_state_idle());

  // First portal detection attempts determines PORTAL state.
  ASSERT_TRUE(start_detection_if_idle());
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_FALSE(start_detection_if_idle());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(is_state_checking_for_portal());
  CompleteURLFetch(net::OK, 200, NULL);

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   kStubWireless1);
  ASSERT_TRUE(is_state_idle());
}

TEST_F(NetworkPortalDetectorImplTest, RequestTimeouts) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  set_lazy_check_interval(base::TimeDelta());

  SetNetworkDeviceEnabled(shill::kTypeWifi, false);
  SetConnected(kStubCellular);

  // First portal detection attempt for cellular1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(1, 5, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  // Second portal detection attempt for cellular1 uses 10sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(2, 10, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  // Third portal detection attempt for cellular1 uses 15sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(3, 15, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  ASSERT_TRUE(is_state_idle());

  // Check that in lazy detection for cellular1 15sec timeout is used.
  enable_lazy_detection();
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  disable_lazy_detection();
  CheckRequestTimeoutAndCompleteAttempt(3, 15, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_idle());

  SetNetworkDeviceEnabled(shill::kTypeWifi, true);
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(1, 5, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  // Second portal detection attempt for wifi1 also uses 5sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(2, 10, net::OK, 204);
  ASSERT_TRUE(is_state_idle());

  // Check that in lazy detection for wifi1 5sec timeout is used.
  enable_lazy_detection();
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  disable_lazy_detection();
  CheckRequestTimeoutAndCompleteAttempt(3, 15, net::OK, 204);
  ASSERT_TRUE(is_state_idle());
}

TEST_F(NetworkPortalDetectorImplTest, StartDetectionIfIdle) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(1, 5, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 10sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(2, 10, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 15sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(3, 15, net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_idle());
  start_detection_if_idle();

  ASSERT_TRUE(is_state_portal_detection_pending());

  // First portal detection attempt for wifi1 uses 5sec timeout.
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(1, 5, net::OK, 204);
  ASSERT_TRUE(is_state_idle());
}

}  // namespace chromeos
