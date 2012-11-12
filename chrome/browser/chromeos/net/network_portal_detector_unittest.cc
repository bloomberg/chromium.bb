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
#include "net/url_request/test_url_fetcher_factory.h"

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

    wifi_network_ = network_library_->FindNetworkByPath("wifi1");
    DCHECK(wifi_network_);

    profile_.reset(new TestingProfile());
    network_portal_detector_.reset(
        new NetworkPortalDetector(profile_->GetRequestContext()));
    network_portal_detector_->Init();

    set_detector(network_portal_detector_->captive_portal_detector_.get());
  }

  virtual void TearDown() {
    network_portal_detector_->Shutdown();
    CrosLibrary::Shutdown();
  }

  NetworkLibrary* network_library() { return network_library_; }
  Network* ethernet_network() { return ethernet_network_; }
  Network* wifi_network() { return wifi_network_; }

  Profile* profile() { return profile_.get(); }

  NetworkPortalDetector* network_portal_detector() {
    return network_portal_detector_.get();
  }

  net::TestURLFetcher* fetcher() { return factory_.GetFetcherByID(0); }

  NetworkPortalDetector::State state() {
    return network_portal_detector()->state();
  }

  bool is_state_idle() {
    return (NetworkPortalDetector::STATE_IDLE == state());
  }

  bool is_state_checking_for_portal() {
    return (NetworkPortalDetector::STATE_CHECKING_FOR_PORTAL == state());
  }

  bool is_state_checking_for_portal_network_changed() {
    return (NetworkPortalDetector::STATE_CHECKING_FOR_PORTAL_NETWORK_CHANGED ==
            state());
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

  // Pointer to fake ethernet network.
  Network* ethernet_network_;

  // Pointer to fake wifi network.
  Network* wifi_network_;

  MessageLoop message_loop_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<NetworkPortalDetector> network_portal_detector_;

  net::TestURLFetcherFactory factory_;
};

TEST_F(NetworkPortalDetectorTest, NoPortal) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_UNKNOWN,
            network_portal_detector()->GetCaptivePortalState(wifi_network()));

  fetcher()->set_response_code(204);
  OnURLFetchComplete(fetcher());

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_ONLINE,
            network_portal_detector()->GetCaptivePortalState(
                wifi_network()));
}

TEST_F(NetworkPortalDetectorTest, Portal) {
  ASSERT_TRUE(is_state_idle());

  // Check HTTP 200 response code.
  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  fetcher()->set_response_code(200);
  OnURLFetchComplete(fetcher());

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_PORTAL,
            network_portal_detector()->GetCaptivePortalState(wifi_network()));

  // Check HTTP 301 response code.
  SetConnected(ethernet_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  fetcher()->set_response_code(301);
  OnURLFetchComplete(fetcher());

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_PORTAL,
            network_portal_detector()->GetCaptivePortalState(
                ethernet_network()));

  // Check HTTP 302 response code.
  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  fetcher()->set_response_code(302);
  OnURLFetchComplete(fetcher());

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_PORTAL,
            network_portal_detector()->GetCaptivePortalState(
                ethernet_network()));
}

TEST_F(NetworkPortalDetectorTest, TwoNetworks) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  // wifi is in portal state.
  fetcher()->set_response_code(200);
  OnURLFetchComplete(fetcher());
  ASSERT_TRUE(is_state_idle());

  SetConnected(ethernet_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  fetcher()->set_response_code(204);
  OnURLFetchComplete(fetcher());
  ASSERT_TRUE(is_state_idle());

  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_ONLINE,
            network_portal_detector()->GetCaptivePortalState(
                ethernet_network()));
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_PORTAL,
            network_portal_detector()->GetCaptivePortalState(wifi_network()));
}

TEST_F(NetworkPortalDetectorTest, NetworkChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  // Active network is changed during portal detection for wifi.
  SetConnected(ethernet_network());
  ASSERT_TRUE(is_state_checking_for_portal_network_changed());

  // WiFi is in portal state and URL fetch is completed, but
  // NetworkPortalDetector should ignore results and retry portal
  // detection.
  fetcher()->set_response_code(200);
  OnURLFetchComplete(fetcher());
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  fetcher()->set_response_code(204);
  OnURLFetchComplete(fetcher());
  ASSERT_TRUE(is_state_idle());

  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_ONLINE,
            network_portal_detector()->GetCaptivePortalState(
                ethernet_network()));

  // As active network was changed during portal detection, we can't
  // rely on URL fetcher response. So, state for wifi network must be
  // unknown.
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_UNKNOWN,
            network_portal_detector()->GetCaptivePortalState(wifi_network()));
}

TEST_F(NetworkPortalDetectorTest, NetworkStateNotChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_checking_for_portal());

  fetcher()->set_response_code(204);
  OnURLFetchComplete(fetcher());

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATE_ONLINE,
            network_portal_detector()->GetCaptivePortalState(wifi_network()));
  SetConnected(wifi_network());
  ASSERT_TRUE(is_state_idle());
}

TEST_F(NetworkPortalDetectorTest, NetworkStateChanged) {
  // TODO (ygorshenin): currently portal detection isn't invoked if
  // previous detection results are PORTAL or ONLINE. We're planning
  // to change this behaviour in future, so, there should be
  // corresponding test.
}

}  // namespace chromeos
