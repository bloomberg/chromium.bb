// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/network_listener.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ConnectionTypeObserver =
    net::NetworkChangeNotifier::ConnectionTypeObserver;
using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

namespace download {

// NetworkChangeNotifier that can change network type in tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        conn_type_(ConnectionType::CONNECTION_UNKNOWN) {}

  // net::NetworkChangeNotifier implementation.
  ConnectionType GetCurrentConnectionType() const override {
    return conn_type_;
  }

  // Change the network type.
  void ChangeNetworkType(ConnectionType type) {
    conn_type_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        type);
    base::RunLoop().RunUntilIdle();
  }

 private:
  ConnectionType conn_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

class MockObserver : public NetworkListener::Observer {
 public:
  MOCK_METHOD1(OnNetworkChange, void(NetworkListener::NetworkStatus));
};

class NetworkListenerTest : public testing::Test {
 public:
  // Simulates a network change call.
  void ChangeNetworkType(ConnectionType type) {
    test_network_notifier_.ChangeNetworkType(type);
  }

 protected:
  NetworkListener network_listener_;
  MockObserver mock_observer_;

  // Needed for network change notifier.
  base::MessageLoop message_loop_;
  TestNetworkChangeNotifier test_network_notifier_;
};

TEST_F(NetworkListenerTest, NotifyObserverNetworkChange) {
  network_listener_.Start();
  network_listener_.AddObserver(&mock_observer_);

  // Initial states check.
  EXPECT_EQ(NetworkListener::NetworkStatus::DISCONNECTED,
            network_listener_.CurrentNetworkStatus());

  // Network switch between mobile networks, the observer should be notified
  // only once.
  EXPECT_CALL(mock_observer_,
              OnNetworkChange(NetworkListener::NetworkStatus::METERED))
      .Times(1)
      .RetiresOnSaturation();

  ChangeNetworkType(ConnectionType::CONNECTION_4G);
  ChangeNetworkType(ConnectionType::CONNECTION_3G);
  ChangeNetworkType(ConnectionType::CONNECTION_2G);
  EXPECT_EQ(NetworkListener::NetworkStatus::METERED,
            network_listener_.CurrentNetworkStatus());

  // Network is switched between wifi and ethernet, the observer should be
  // notified only once.
  EXPECT_CALL(mock_observer_,
              OnNetworkChange(NetworkListener::NetworkStatus::UNMETERED))
      .Times(1)
      .RetiresOnSaturation();

  ChangeNetworkType(ConnectionType::CONNECTION_WIFI);
  ChangeNetworkType(ConnectionType::CONNECTION_ETHERNET);
}

}  // namespace download
