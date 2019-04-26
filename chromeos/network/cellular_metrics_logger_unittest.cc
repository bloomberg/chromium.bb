// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/login/login_state/login_state.h"
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
const char kActivationStatusAtLoginHistogram[] =
    "Network.Cellular.Activation.StatusAtLogin";
const char kTimeToConnectedHistogram[] =
    "Network.Cellular.Connection.TimeToConnected";

}  // namespace

class CellularMetricsLoggerTest : public testing::Test {
 public:
  CellularMetricsLoggerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::NowSource::
                MAIN_THREAD_MOCK_TIME) {}
  ~CellularMetricsLoggerTest() override = default;

  void SetUp() override {
    LoginState::Initialize();

    network_metrics_logger_.reset(new CellularMetricsLogger());
    network_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler());
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    network_metrics_logger_.reset();
    LoginState::Shutdown();
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

    service_client_test()->SetServiceProperty(
        kTestCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
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

TEST_F(CellularMetricsLoggerTest, CellularActivationStateAtLoginTest) {
  base::HistogramTester histogram_tester;

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 0);
  scoped_task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kNotActivated, 1);

  // Should log immediately when networks are already initialized.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kNotActivated, 2);

  // Should log activated state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  base::RunLoop().RunUntilIdle();
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kActivated, 1);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK_APP);
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularTimeToConnectedTest) {
  constexpr base::TimeDelta kTestConnectionTime =
      base::TimeDelta::FromMilliseconds(321);
  InitCellular();
  base::HistogramTester histogram_tester;
  const base::Value kOnlineStateValue(shill::kStateOnline);
  const base::Value kAssocStateValue(shill::kStateAssociation);

  // Should not log connection time when not activated.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();
  scoped_task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kTimeToConnectedHistogram, 0);

  // Should log connection time when activated.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();
  scoped_task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(kTimeToConnectedHistogram,
                                         kTestConnectionTime, 1);
}

}  // namespace chromeos
