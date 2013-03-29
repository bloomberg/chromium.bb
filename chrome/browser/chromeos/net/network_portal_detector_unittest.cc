// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "chrome/browser/captive_portal/testing_utils.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/network_library_impl_base.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/test/base/testing_profile.h"
#include "net/base/net_errors.h"

namespace chromeos {

class NetworkPortalDetectorTest
    : public testing::Test,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorTest() {}
  virtual ~NetworkPortalDetectorTest() {}

  virtual void SetUp() {
    CrosLibrary::Initialize(true);
    network_library_ = CrosLibrary::Get()->GetNetworkLibrary();
    DCHECK(network_library_);

    ethernet_network_ = network_library_->FindNetworkByPath("eth1");
    DCHECK(ethernet_network_);

    wifi1_network_ = network_library_->FindNetworkByPath("wifi1");
    DCHECK(wifi1_network_);

    wifi2_network_ = network_library_->FindNetworkByPath("wifi2");
    DCHECK(wifi2_network_);

    profile_.reset(new TestingProfile());
    network_portal_detector_.reset(
        new NetworkPortalDetector(profile_->GetRequestContext()));
    network_portal_detector_->Init();
    network_portal_detector_->set_enabled(true);

    set_detector(network_portal_detector_->captive_portal_detector_.get());

    // Prevents flakiness due to message loop delays.
    set_time_ticks(base::TimeTicks::Now());
  }

  virtual void TearDown() {
    network_portal_detector_->Shutdown();
    CrosLibrary::Shutdown();
  }

  void CheckPortalState(NetworkPortalDetector::CaptivePortalStatus status,
                        int response_code,
                        const Network* network) {
    NetworkPortalDetector::CaptivePortalState state =
        network_portal_detector()->GetCaptivePortalState(network);
    ASSERT_EQ(status, state.status);
    ASSERT_EQ(response_code, state.response_code);
  }

  NetworkLibrary* network_library() { return network_library_; }
  Network* ethernet_network() { return ethernet_network_; }
  Network* wifi1_network() { return wifi1_network_; }
  Network* wifi2_network() { return wifi2_network_; }

  Profile* profile() { return profile_.get(); }

  NetworkPortalDetector* network_portal_detector() {
    return network_portal_detector_.get();
  }

  NetworkPortalDetector::State state() {
    return network_portal_detector()->state();
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

  bool is_state_idle() {
    return (NetworkPortalDetector::STATE_IDLE == state());
  }

  bool is_state_portal_detection_pending() {
    return (NetworkPortalDetector::STATE_PORTAL_CHECK_PENDING == state());
  }

  bool is_state_checking_for_portal() {
    return (NetworkPortalDetector::STATE_CHECKING_FOR_PORTAL == state());
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

  void set_min_time_between_attempts(const base::TimeDelta& delta) {
    network_portal_detector()->set_min_time_between_attempts_for_testing(delta);
  }

  void set_lazy_check_interval(const base::TimeDelta& delta) {
    network_portal_detector()->set_lazy_check_interval_for_testing(delta);
  }

  void set_time_ticks(const base::TimeTicks& time_ticks) {
    network_portal_detector()->set_time_ticks_for_testing(time_ticks);
  }

  void SetBehindPortal(Network* network) {
    Network::TestApi test_api(network);
        test_api.SetBehindPortal();
    static_cast<NetworkLibraryImplBase*>(
        network_library())->CallConnectToNetwork(network);
    MessageLoop::current()->RunUntilIdle();
  }

  void SetConnected(Network* network) {
    Network::TestApi test_api(network);
    test_api.SetConnected();
    static_cast<NetworkLibraryImplBase*>(
        network_library())->CallConnectToNetwork(network);
    MessageLoop::current()->RunUntilIdle();
  }

 private:
  NetworkLibrary* network_library_;

  // Pointer to a fake ethernet network.
  Network* ethernet_network_;

  // Pointer to a fake wifi1 network.
  Network* wifi1_network_;

  // Pointer to a fake wifi2 network.
  Network* wifi2_network_;

  MessageLoop message_loop_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<NetworkPortalDetector> network_portal_detector_;
};

TEST_F(NetworkPortalDetectorTest, NoPortal) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi1_network());

  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, Portal) {
  ASSERT_TRUE(is_state_idle());

  // Check HTTP 200 response code.
  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());

  // Check HTTP 301 response code.
  SetConnected(wifi2_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 301, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 301,
                   wifi2_network());

  // Check HTTP 302 response code.
  SetConnected(ethernet_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 302, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 302,
                   ethernet_network());
}

TEST_F(NetworkPortalDetectorTest, TwoNetworks) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  // wifi is in portal state.
  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_TRUE(is_state_idle());

  SetConnected(ethernet_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   ethernet_network());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, NetworkChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi1_network());

  // WiFi is in portal state.
  fetcher()->set_response_code(200);
  ASSERT_TRUE(is_state_checking_for_portal());

  // Active network is changed during portal detection for wifi.
  SetConnected(ethernet_network());

  // Portal detection for wifi is cancelled, portal detection for
  // ethernet is initiated.
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   ethernet_network());

  // As active network was changed during portal detection for wifi
  // network, it's state must be unknown.
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, NetworkStateNotChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   wifi1_network());

  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_idle());
}

TEST_F(NetworkPortalDetectorTest, NetworkStateChanged) {
  // Test for Portal -> Online -> Portal network state transitions.
  ASSERT_TRUE(is_state_idle());

  SetBehindPortal(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());

  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   wifi1_network());

  SetBehindPortal(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, PortalDetectionTimeout) {
  ASSERT_TRUE(is_state_idle());

  // For instantaneous timeout.
  set_request_timeout(base::TimeDelta::FromSeconds(0));

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(wifi1_network());
  MessageLoop::current()->RunUntilIdle();

  // First portal detection timeouts, next portal detection is
  // scheduled.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorTest, PortalDetectionRetryAfter) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 101\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(wifi1_network());
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 101 seconds.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(101), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorTest, PortalDetectorRetryAfterIsSmall) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 1\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(wifi1_network());
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 3 seconds (due to minimum time between detection
  // attemps).
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorTest, FirstAttemptFailed) {
  ASSERT_TRUE(is_state_idle());

  set_min_time_between_attempts(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(wifi1_network());

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(2, attempt_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, AllAttemptsFailed) {
  ASSERT_TRUE(is_state_idle());

  set_min_time_between_attempts(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(wifi1_network());

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(2, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(3, attempt_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 503,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, ProxyAuthRequired) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetConnected(wifi1_network());
  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED, 407,
      wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, NoResponseButBehindPortal) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetBehindPortal(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, LazyDetectionForOnlineNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  set_lazy_check_interval(base::TimeDelta());

  SetConnected(wifi1_network());
  enable_lazy_detection();
  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  disable_lazy_detection();

  // One more detection result, because DizableLazyDetection() doesn't
  // cancel last detection request.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
      wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, LazyDetectionForPortalNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());
  set_lazy_check_interval(base::TimeDelta());

  SetConnected(wifi1_network());
  enable_lazy_detection();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  MessageLoop::current()->RunUntilIdle();

  disable_lazy_detection();

  // One more detection result, because DizableLazyDetection() doesn't
  // cancel last detection request.
  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                   wifi1_network());
}

TEST_F(NetworkPortalDetectorTest, DetectionTimeoutIsCancelled) {
  ASSERT_TRUE(is_state_idle());
  set_min_time_between_attempts(base::TimeDelta());

  SetConnected(wifi1_network());
  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());

  cancel_portal_detection();

  ASSERT_TRUE(is_state_idle());
  ASSERT_TRUE(detection_timeout_is_cancelled());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1,
                   wifi1_network());
}

}  // namespace chromeos
