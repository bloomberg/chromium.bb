// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/net/wifi_access_point_info_provider.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/nqe/effective_connection_type.h"

namespace net {
class NetworkQualityEstimator;
}

namespace metrics {

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class NetworkMetricsProvider
    : public MetricsProvider,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Class that provides |this| with the network quality estimator.
  class NetworkQualityEstimatorProvider {
   public:
    virtual ~NetworkQualityEstimatorProvider() {}

    // Returns the network quality estimator. May be nullptr.
    virtual net::NetworkQualityEstimator* GetNetworkQualityEstimator() = 0;

    // Returns the task runner on which |this| should be used and destroyed.
    virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() = 0;

   protected:
    NetworkQualityEstimatorProvider() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimatorProvider);
  };

  // Creates a NetworkMetricsProvider, where
  // |network_quality_estimator_provider| should be set if it is useful to
  // attach the quality of the network to the metrics report.
  explicit NetworkMetricsProvider(
      std::unique_ptr<NetworkQualityEstimatorProvider>
          network_quality_estimator_provider = nullptr);
  ~NetworkMetricsProvider() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest, EffectiveConnectionType);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ECTAmbiguousOnConnectionTypeChange);

  // Listens to the changes in the effective conection type.
  class EffectiveConnectionTypeObserver;

  // MetricsProvider:
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

  // ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  SystemProfileProto::Network::ConnectionType GetConnectionType() const;
  SystemProfileProto::Network::WifiPHYLayerProtocol GetWifiPHYLayerProtocol()
      const;
  SystemProfileProto::Network::EffectiveConnectionType
  GetEffectiveConnectionType() const;

  // Posts a call to net::GetWifiPHYLayerProtocol on the blocking pool.
  void ProbeWifiPHYLayerProtocol();
  // Callback from the blocking pool with the result of
  // net::GetWifiPHYLayerProtocol.
  void OnWifiPHYLayerProtocolResult(net::WifiPHYLayerProtocol mode);

  // Writes info about the wireless access points that this system is
  // connected to.
  void WriteWifiAccessPointProto(
      const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
      SystemProfileProto::Network* network_proto);

  // Logs metrics that are functions of other metrics being uploaded.
  void LogAggregatedMetrics();

  // Notifies |this| that the effective connection type of the current network
  // has changed to |type|.
  void OnEffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to net::NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // True if |wifi_phy_layer_protocol_| changed during the lifetime of the log.
  bool wifi_phy_layer_protocol_is_ambiguous_;
  // The PHY mode of the currently associated access point obtained via
  // net::GetWifiPHYLayerProtocol.
  net::WifiPHYLayerProtocol wifi_phy_layer_protocol_;

  // Helper object for retrieving connected wifi access point information.
  std::unique_ptr<WifiAccessPointInfoProvider> wifi_access_point_info_provider_;

  // These metrics track histogram totals for the Net.ErrorCodesForMainFrame3
  // histogram. They are used to compute deltas at upload time.
  base::HistogramBase::Count total_aborts_;
  base::HistogramBase::Count total_codes_;

  // Provides the network quality estimator. May be null.
  std::unique_ptr<NetworkQualityEstimatorProvider>
      network_quality_estimator_provider_;

  // Listens to the changes in the effective connection type. Initialized and
  // destroyed using |network_quality_task_runner_|. May be null.
  std::unique_ptr<EffectiveConnectionTypeObserver>
      effective_connection_type_observer_;

  // Task runner using which |effective_connection_type_observer_| is
  // initialized and destroyed. May be null.
  scoped_refptr<base::SequencedTaskRunner> network_quality_task_runner_;

  // Last known effective connection type.
  net::EffectiveConnectionType effective_connection_type_;

  // True if |effective_connection_type_| changed during the lifetime of the
  // log.
  bool effective_connection_type_is_ambiguous_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
