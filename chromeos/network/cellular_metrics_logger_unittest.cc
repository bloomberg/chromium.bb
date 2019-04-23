// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestCellularServicePath[] = "/service/cellular0";
const char kTestEthServicePath[] = "/service/eth0";

const char kUsageCountHistogram[] = "Network.Cellular.Usage.Count";

}  // namespace

class CellularMetricsLoggerTest : public testing::Test {
 public:
  CellularMetricsLoggerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME) {}
  ~CellularMetricsLoggerTest() override = default;

  void SetUp() override {
    network_metrics_logger_.reset(new CellularMetricsLogger());
    network_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler());
  }

 protected:
  void InitEthernet() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestEthServicePath, "test_guid1", "test_eth",
                             shill::kTypeEthernet, shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();
  }

  void InitCellular() {
    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->AddDevice(kTestCellularDevicePath, shill::kTypeCellular,
                           "test_cellular_device");

    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestCellularServicePath, "test_guid0",
                             "test_cellular", shill::kTypeCellular,
                             shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveCellular() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->RemoveService(kTestCellularServicePath);

    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->RemoveDevice(kTestCellularDevicePath);
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  ShillServiceClient::TestInterface* service_client_test() {
    return network_state_test_helper_.service_test();
  }

 private:
  NetworkStateTestHelper network_state_test_helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<CellularMetricsLogger> network_metrics_logger_;

  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLoggerTest);
};

TEST_F(CellularMetricsLoggerTest, CellularUsageCountTest) {
  InitEthernet();
  InitCellular();
  base::HistogramTester histogram_tester;
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state until after timeout.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  scoped_task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      1);

  // Cellular connected with other network.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Cellular not connected.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      2);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountDongleTest) {
  InitEthernet();
  base::HistogramTester histogram_tester;
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state if no cellular devices are available.
  scoped_task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  // Should log state if a new cellular device is plugged in.
  InitCellular();
  scoped_task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      1);

  // Should log connected state for cellular device that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Should log connected as only network state for cellular device
  // that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Should not log state if cellular device is unplugged.
  RemoveCellular();
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 3);
}

}  // namespace chromeos
