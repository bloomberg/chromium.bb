// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_resource/eula_accepted_notifier.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_resource {

// Override NetworkChangeNotifier to simulate connection type changes for tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : connection_type_(net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

  // Simulates a change of the connection type to |type|. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    connection_type_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        connection_type_);
    base::RunLoop().RunUntilIdle();
  }

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// EulaAcceptedNotifier test class that allows mocking the EULA accepted state
// and issuing simulated notifications.
class TestEulaAcceptedNotifier : public EulaAcceptedNotifier {
 public:
  TestEulaAcceptedNotifier()
      : EulaAcceptedNotifier(nullptr),
        eula_accepted_(false) {
  }
  ~TestEulaAcceptedNotifier() override {}

  bool IsEulaAccepted() override { return eula_accepted_; }

  void SetEulaAcceptedForTesting(bool eula_accepted) {
    eula_accepted_ = eula_accepted;
  }

  void SimulateEulaAccepted() {
    NotifyObserver();
  }

 private:
  bool eula_accepted_;

  DISALLOW_COPY_AND_ASSIGN(TestEulaAcceptedNotifier);
};

enum class ConnectionTrackerResponseMode {
  kSynchronous,
  kAsynchronous,
};

enum class MigrationState {
  kEnabled,
  kDisabled,
};

struct TestCase {
  ConnectionTrackerResponseMode response_mode;
  MigrationState migration_state;
};

// A test fixture class for ResourceRequestAllowedNotifier tests that require
// network state simulations. This also acts as the service implementing the
// ResourceRequestAllowedNotifier::Observer interface.
class ResourceRequestAllowedNotifierTest
    : public testing::Test,
      public ResourceRequestAllowedNotifier::Observer,
      public testing::WithParamInterface<TestCase> {
 public:
  ResourceRequestAllowedNotifierTest()
      : network_tracker_(GetParam().response_mode ==
                             ConnectionTrackerResponseMode::kSynchronous,
                         network::mojom::ConnectionType::CONNECTION_WIFI),
        resource_request_allowed_notifier_(&prefs_, &network_tracker_),
        eula_notifier_(new TestEulaAcceptedNotifier),
        was_notified_(false) {
    if (GetParam().migration_state == MigrationState::kEnabled)
      feature_list_.InitAndEnableFeature(kResourceRequestAllowedMigration);
    resource_request_allowed_notifier_.InitWithEulaAcceptNotifier(
        this, base::WrapUnique(eula_notifier_));
  }
  ~ResourceRequestAllowedNotifierTest() override {}

  bool was_notified() const { return was_notified_; }

  // ResourceRequestAllowedNotifier::Observer override:
  void OnResourceRequestsAllowed() override { was_notified_ = true; }

  void SimulateNetworkConnectionChange(network::mojom::ConnectionType type) {
    if (GetParam().migration_state == MigrationState::kEnabled) {
      network_tracker_.SetConnectionType(type);
      base::RunLoop().RunUntilIdle();
    } else {
      network_notifier_.SimulateNetworkConnectionChange(
          net::NetworkChangeNotifier::ConnectionType(type));
    }
  }

  // Simulate a resource request from the test service. It returns true if
  // resource request is allowed. Otherwise returns false and will change the
  // result of was_notified() to true when the request is allowed.
  bool SimulateResourceRequest() {
    return resource_request_allowed_notifier_.ResourceRequestsAllowed();
  }

  void SimulateEulaAccepted() {
    eula_notifier_->SimulateEulaAccepted();
  }

  // Eula manipulation methods:
  void SetNeedsEulaAcceptance(bool needs_acceptance) {
    eula_notifier_->SetEulaAcceptedForTesting(!needs_acceptance);
  }

  void SetWaitingForEula(bool waiting) {
    resource_request_allowed_notifier_.SetWaitingForEulaForTesting(waiting);
  }

  // Used in tests involving the EULA. Disables both the EULA accepted state
  // and the network.
  void DisableEulaAndNetwork() {
    SimulateNetworkConnectionChange(
        network::mojom::ConnectionType::CONNECTION_NONE);
    SetWaitingForEula(true);
    SetNeedsEulaAcceptance(true);
  }

  void SetUp() override {
    // Assume the test service has already requested permission, as all tests
    // just test that criteria changes notify the server.
    // Set default EULA state to done (not waiting and EULA accepted) to
    // simplify non-ChromeOS tests.
    SetWaitingForEula(false);
    SetNeedsEulaAcceptance(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::MessageLoopForUI message_loop_;
  network::TestNetworkConnectionTracker network_tracker_;
  TestRequestAllowedNotifier resource_request_allowed_notifier_;
  TestNetworkChangeNotifier network_notifier_;
  TestingPrefServiceSimple prefs_;
  TestEulaAcceptedNotifier* eula_notifier_;  // Weak, owned by RRAN.
  bool was_notified_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestAllowedNotifierTest);
};

TEST_P(ResourceRequestAllowedNotifierTest, NotifyOnInitialNetworkState) {
  if (GetParam().response_mode == ConnectionTrackerResponseMode::kSynchronous) {
    EXPECT_TRUE(SimulateResourceRequest());
  } else {
    EXPECT_FALSE(SimulateResourceRequest());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_notified());
  }
}

TEST_P(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOffline) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOnlineToOnline) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NotifyOnReconnect) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_TRUE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NoNotifyOnWardriving) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NoNotifyOnFlakyConnection) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NotifyOnFlakyConnection) {
  // First, the observer queries the state while the network is connected.
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());

  // Now, the observer queries the state while the network is disconnected.
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NoNotifyOnEulaAfterGoOffline) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NoRequestNoNotify) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria are met. Note that this is done by not
  // calling SimulateResourceRequest here.
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, EulaOnlyNetworkOffline) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, EulaFirst) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NetworkFirst) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_TRUE(was_notified());
}

TEST_P(ResourceRequestAllowedNotifierTest, NoRequestNoNotifyEula) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria are met. Note that this is done by not
  // calling SimulateResourceRequest here.
  DisableEulaAndNetwork();

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ResourceRequestAllowedNotifierTest,
    testing::Values(TestCase({ConnectionTrackerResponseMode::kSynchronous,
                              MigrationState::kEnabled}),
                    TestCase({ConnectionTrackerResponseMode::kAsynchronous,
                              MigrationState::kEnabled}),
                    TestCase({ConnectionTrackerResponseMode::kSynchronous,
                              MigrationState::kDisabled})));

}  // namespace web_resource
