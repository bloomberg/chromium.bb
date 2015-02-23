// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/net/wifi_access_point_info_provider.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"

namespace metrics {

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class NetworkMetricsProvider
    : public MetricsProvider,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Creates a NetworkMetricsProvider, where |io_task_runner| is used to post
  // network info collection tasks.
  explicit NetworkMetricsProvider(base::TaskRunner* io_task_runner);
  ~NetworkMetricsProvider() override;

  // Returns callback function bound to the weak pointer of the provider, which
  // can be used to get whether current connection type is cellular.
  base::Callback<void(bool*)> GetConnectionCallback();

 private:
  // MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

  // ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  SystemProfileProto::Network::ConnectionType GetConnectionType() const;
  SystemProfileProto::Network::WifiPHYLayerProtocol GetWifiPHYLayerProtocol()
      const;

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

  // Returns true if the connection type is 2G, 3G, or 4G.
  bool IsCellularConnection();

  // Assigns the passed |is_cellular_out| parameter based on whether current
  // network connection is cellular.
  void GetIsCellularConnection(bool* is_cellular_out);

  // Task runner used for blocking file I/O.
  base::TaskRunner* io_task_runner_;

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
  scoped_ptr<WifiAccessPointInfoProvider> wifi_access_point_info_provider_;

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
