// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#endif  // OS_CHROMEOS

namespace metrics {

namespace {

class TestNetworkQualityEstimatorProvider
    : public NetworkMetricsProvider::NetworkQualityEstimatorProvider {
 public:
  explicit TestNetworkQualityEstimatorProvider(
      net::TestNetworkQualityEstimator* estimator)
      : estimator_(estimator) {}
  ~TestNetworkQualityEstimatorProvider() override {}

 private:
  // NetworkMetricsProvider::NetworkQualityEstimatorProvider:
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  void PostReplyNetworkQualityEstimator(
      base::Callback<void(net::NetworkQualityEstimator*)> callback) override {
    callback.Run(estimator_);
  }

  net::TestNetworkQualityEstimator* estimator_;
  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimatorProvider);
};

}  // namespace

class NetworkMetricsProviderTest : public testing::Test {
 protected:
  NetworkMetricsProviderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
#endif  // OS_CHROMEOS
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

// Verifies that the effective connection type is correctly set.
TEST_F(NetworkMetricsProviderTest, EffectiveConnectionType) {
  net::TestNetworkQualityEstimator estimator;
  std::unique_ptr<NetworkMetricsProvider::NetworkQualityEstimatorProvider>
      estimator_provider(base::WrapUnique(
          new TestNetworkQualityEstimatorProvider(&estimator)));
  SystemProfileProto system_profile;
  NetworkMetricsProvider network_metrics_provider(
      std::move(estimator_provider));

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            system_profile.network().max_effective_connection_type());

  // Set RTT so that the effective connection type is computed as 2G.
  estimator.set_recent_http_rtt(base::TimeDelta::FromMilliseconds(1500));
  estimator.set_start_time_null_http_rtt(
      base::TimeDelta::FromMilliseconds(1500));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);
  // Running a request would cause the effective connection type to be computed
  // as 2G, and observers to be notified.
  estimator.RunOneRequest();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Set RTT so that the effective connection type is computed as SLOW_2G.
  estimator.set_recent_http_rtt(base::TimeDelta::FromMilliseconds(3000));
  estimator.set_start_time_null_http_rtt(
      base::TimeDelta::FromMilliseconds(3000));
  // Running a request would cause the effective connection type to be computed
  // as SLOW_2G, and observers to be notified.
  estimator.RunOneRequest();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  // Effective connection type changed from 2G to SLOW_2G during the lifetime of
  // the log. Minimum value of ECT must be different from the maximum value.
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Getting the system profile again should return the current effective
  // connection type.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().max_effective_connection_type());
}

// Verifies that the effective connection type is not set to UNKNOWN when there
// is a change in the connection type.
TEST_F(NetworkMetricsProviderTest, ECTAmbiguousOnConnectionTypeChange) {
  net::TestNetworkQualityEstimator estimator;
  std::unique_ptr<NetworkMetricsProvider::NetworkQualityEstimatorProvider>
      estimator_provider(base::WrapUnique(
          new TestNetworkQualityEstimatorProvider(&estimator)));
  SystemProfileProto system_profile;
  NetworkMetricsProvider network_metrics_provider(
      std::move(estimator_provider));

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);

  // Set RTT so that the effective connection type is computed as 2G.
  estimator.set_recent_http_rtt(base::TimeDelta::FromMilliseconds(1500));
  estimator.set_start_time_null_http_rtt(
      base::TimeDelta::FromMilliseconds(1500));
  // Running a request would cause the effective connection type to be computed
  // as 2G, and observers to be notified.
  estimator.RunOneRequest();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);

  // There is no change in the connection type. Effective connection types
  // should be reported as 2G.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Even with change in the connection type, effective connection types
  // should be reported as 2G.
  network_metrics_provider.OnConnectionTypeChanged(
      net::NetworkChangeNotifier::CONNECTION_2G);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());
}

// Verifies that the effective connection type is not set to UNKNOWN when the
// connection type is OFFLINE.
TEST_F(NetworkMetricsProviderTest, ECTNotAmbiguousOnOffline) {
  for (net::EffectiveConnectionType force_ect :
       {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE}) {
    std::unique_ptr<net::NetworkQualityEstimatorParams> params =
        base::MakeUnique<net::NetworkQualityEstimatorParams>(
            std::map<std::string, std::string>());
    net::NetworkQualityEstimatorParams* params_ptr = params.get();
    net::TestNetworkQualityEstimator estimator(std::move(params));

    std::unique_ptr<NetworkMetricsProvider::NetworkQualityEstimatorProvider>
        estimator_provider(base::WrapUnique(
            new TestNetworkQualityEstimatorProvider(&estimator)));
    SystemProfileProto system_profile;
    NetworkMetricsProvider network_metrics_provider(
        std::move(estimator_provider));

    params_ptr->SetForcedEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_2G);
    estimator.RunOneRequest();

    params_ptr->SetForcedEffectiveConnectionType(force_ect);
    estimator.RunOneRequest();
    network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
              system_profile.network().min_effective_connection_type());
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
              system_profile.network().max_effective_connection_type());

    params_ptr->SetForcedEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_4G);
    estimator.RunOneRequest();

    network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
              system_profile.network().min_effective_connection_type());
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
              system_profile.network().max_effective_connection_type());
  }
}

}  // namespace metrics