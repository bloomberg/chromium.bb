// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/metrics/variations/resource_request_allowed_notifier_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Override NetworkChangeNotifier to simulate connection type changes for tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        connection_type_to_return_(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {
  }

  // Simulates a change of the connection type to |type|. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    connection_type_to_return_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
    MessageLoop::current()->RunUntilIdle();
  }

 private:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// A test fixture class for ResourceRequestAllowedNotifier tests that require
// network state simulations. This also acts as the service implementing the
// ResourceRequestAllowedNotifier::Observer interface.
class ResourceRequestAllowedNotifierTest
    : public testing::Test,
      public ResourceRequestAllowedNotifier::Observer {
 public:
  ResourceRequestAllowedNotifierTest()
    : ui_thread(content::BrowserThread::UI, &message_loop),
      was_notified_(false) {
#if defined(OS_CHROMEOS)
    // Set this flag to true so the Init call sets up the wait on the EULA.
    SetNeedsEulaAcceptance(true);
#endif
    resource_request_allowed_notifier_.Init(this);
  }
  virtual ~ResourceRequestAllowedNotifierTest() { }

  bool was_notified() const { return was_notified_; }

  // ResourceRequestAllowedNotifier::Observer override:
  virtual void OnResourceRequestsAllowed() OVERRIDE {
    was_notified_ = true;
  }

  // Network manipulation methods:
  void SetWasWaitingForNetwork(bool waiting) {
    resource_request_allowed_notifier_.
        SetWasWaitingForNetworkForTesting(waiting);
  }

  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    network_notifier.SimulateNetworkConnectionChange(type);
  }

  // Simulate a resource request from the test service.
  void SimulateResourceRequest() {
    resource_request_allowed_notifier_.ResourceRequestsAllowed();
  }

#if defined(OS_CHROMEOS)
  // Eula manipulation methods:
  void SetNeedsEulaAcceptance(bool needs_acceptance) {
    resource_request_allowed_notifier_.SetNeedsEulaAcceptance(needs_acceptance);
  }

  void SetWasWaitingForEula(bool waiting) {
    resource_request_allowed_notifier_.SetWasWaitingForEulaForTesting(waiting);
  }

  void SimulateEulaAccepted() {
    SetNeedsEulaAcceptance(false);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }

  // Used in tests involving the EULA. Disables both the EULA accepted state
  // and the network.
  void DisableEulaAndNetwork() {
    SetWasWaitingForNetwork(true);
    SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_NONE);
    SetWasWaitingForEula(true);
    SetNeedsEulaAcceptance(true);
  }
#endif

  virtual void SetUp() OVERRIDE {
    // Assume the test service has already requested permission, as all tests
    // just test that criteria changes notify the server.
#if defined(OS_CHROMEOS)
    // Set default EULA state to done (not waiting and EULA accepted) to
    // simplify non-ChromeOS tests.
    SetWasWaitingForEula(false);
    SetNeedsEulaAcceptance(false);
#endif
  }

 private:
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread;
  TestNetworkChangeNotifier network_notifier;
  TestRequestAllowedNotifier resource_request_allowed_notifier_;
  bool was_notified_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestAllowedNotifierTest);
};

TEST_F(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOffline) {
  SimulateResourceRequest();
  SetWasWaitingForNetwork(true);
  SimulateNetworkConnectionChange(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOnlineToOnline) {
  SimulateResourceRequest();
  SetWasWaitingForNetwork(false);
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NotifyOnReconnect) {
  SimulateResourceRequest();
  SetWasWaitingForNetwork(true);
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoNotifyOnWardriving) {
  SimulateResourceRequest();
  SetWasWaitingForNetwork(false);
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_3G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_4G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoNotifyOnFlakyConnection) {
  SimulateResourceRequest();
  SetWasWaitingForNetwork(false);
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoRequestNoNotify) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria is met. Note that this is done by not
  // calling SimulateResourceRequest here.
  SetWasWaitingForNetwork(true);
  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

#if defined(OS_CHROMEOS)
TEST_F(ResourceRequestAllowedNotifierTest, EulaOnlyNetworkOffline) {
  SimulateResourceRequest();
  DisableEulaAndNetwork();

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, EulaFirst) {
  SimulateResourceRequest();
  DisableEulaAndNetwork();

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());

  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NetworkFirst) {
  SimulateResourceRequest();
  DisableEulaAndNetwork();

  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoRequestNoNotifyEula) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria is met. Note that this is done by not
  // calling SimulateResourceRequest here.
  DisableEulaAndNetwork();

  SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}
#endif  // OS_CHROMEOS
