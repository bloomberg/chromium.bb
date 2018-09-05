// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_monitor_metric_observer.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

std::ostream& operator<<(
    std::ostream& os,
    DiscoveryNetworkMonitorConnectionType connection_type) {
  switch (connection_type) {
    case DiscoveryNetworkMonitorConnectionType::kWifi:
      os << "kWifi";
      break;
    case DiscoveryNetworkMonitorConnectionType::kEthernet:
      os << "kEthernet";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsWifi:
      os << "kUnknownReportedAsWifi";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsEthernet:
      os << "kUnknownReportedAsEthernet";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsOther:
      os << "kUnknownReportedAsOther";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknown:
      os << "kUnknown";
      break;
    case DiscoveryNetworkMonitorConnectionType::kDisconnected:
      os << "kDisconnected";
      break;
    default:
      os << "Bad DiscoveryNetworkMonitorConnectionType value";
      break;
  }
  return os;
}

namespace {

using ::testing::_;

class MockMetrics : public DiscoveryNetworkMonitorMetrics {
 public:
  MOCK_METHOD1(RecordTimeBetweenNetworkChangeEvents, void(base::TimeDelta));
  MOCK_METHOD1(RecordConnectionType,
               void(DiscoveryNetworkMonitorConnectionType));
};

class DiscoveryNetworkMonitorMetricObserverTest : public ::testing::Test {
 public:
  DiscoveryNetworkMonitorMetricObserverTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        thread_bundle_(content::TestBrowserThreadBundle::PLAIN_MAINLOOP),
        test_network_connection_tracker_(
            true,
            network::mojom::ConnectionType::CONNECTION_NONE),
        start_ticks_(scoped_task_environment_.NowTicks()),
        metrics_(std::make_unique<MockMetrics>()),
        mock_metrics_(metrics_.get()),
        metric_observer_(scoped_task_environment_.GetMockTickClock(),
                         std::move(metrics_)) {
    content::SetNetworkConnectionTrackerForTesting(
        &test_network_connection_tracker_);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::TimeDelta time_advance_ = base::TimeDelta::FromMilliseconds(10);
  network::TestNetworkConnectionTracker test_network_connection_tracker_;
  const base::TimeTicks start_ticks_;
  std::unique_ptr<MockMetrics> metrics_;
  MockMetrics* mock_metrics_;

  DiscoveryNetworkMonitorMetricObserver metric_observer_;
};

}  // namespace

TEST_F(DiscoveryNetworkMonitorMetricObserverTest, RecordsFirstGoodNetworkWifi) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kWifi));
  metric_observer_.OnNetworksChanged("network1");
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkEthernet) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network1");
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownWifi) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsWifi));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownEthernet) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsEthernet));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownOther) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsOther));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknown) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kUnknown));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkDisconnected) {
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       DoesntRecordEphemeralDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  EXPECT_CALL(*mock_metrics_, RecordConnectionType(_)).Times(0);
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       DoesntRecordEphemeralDisconnectedStateWhenFirst) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(*mock_metrics_, RecordConnectionType(_)).Times(0);
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsTimeChangeBetweenConnectionTypeEvents) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  scoped_task_environment_.FastForwardBy(time_advance_);
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  scoped_task_environment_.FastForwardBy(time_advance_);
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_ * 2) - start_ticks_));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordChangeToDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  scoped_task_environment_.FastForwardBy(time_advance_);
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  scoped_task_environment_.FastForwardBy(time_advance_);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_) - start_ticks_));
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordChangeFromDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  scoped_task_environment_.FastForwardBy(time_advance_);
  const auto disconnect_ticks = scoped_task_environment_.NowTicks();
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  scoped_task_environment_.FastForwardBy(time_advance_);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_) - start_ticks_));
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  scoped_task_environment_.RunUntilIdle();

  scoped_task_environment_.FastForwardBy(time_advance_);
  const auto second_ethernet_ticks = scoped_task_environment_.NowTicks();
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(
                                  second_ethernet_ticks - disconnect_ticks));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  test_network_connection_tracker_.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");
}

}  // namespace media_router
