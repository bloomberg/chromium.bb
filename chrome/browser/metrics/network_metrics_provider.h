// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_NETWORK_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_NETWORK_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/wifi_access_point_info_provider.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class NetworkMetricsProvider
    : public metrics::MetricsProvider,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  NetworkMetricsProvider();
  virtual ~NetworkMetricsProvider();

  // metrics::MetricsProvider:
  virtual void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) OVERRIDE;

  // ConnectionTypeObserver:
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  metrics::SystemProfileProto::Network::ConnectionType
  GetConnectionType() const;
  metrics::SystemProfileProto::Network::WifiPHYLayerProtocol
  GetWifiPHYLayerProtocol() const;

  // Posts a call to net::GetWifiPHYLayerProtocol on the blocking pool.
  void ProbeWifiPHYLayerProtocol();
  // Callback from the blocking pool with the result of
  // net::GetWifiPHYLayerProtocol.
  void OnWifiPHYLayerProtocolResult(net::WifiPHYLayerProtocol mode);

  // Writes info about the wireless access points that this system is
  // connected to.
  void WriteWifiAccessPointProto(
      const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
      metrics::SystemProfileProto::Network* network_proto);

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to net::NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // True if |wifi_phy_layer_protocol_| changed during the lifetime of the log.
  bool wifi_phy_layer_protocol_is_ambiguous_;
  // The PHY mode of the currently associated access point obtained via
  // net::GetWifiPHYLayerProtocol.
  net::WifiPHYLayerProtocol wifi_phy_layer_protocol_;

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_;

  // Helper object for retrieving connected wifi access point information.
  scoped_ptr<WifiAccessPointInfoProvider> wifi_access_point_info_provider_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_NETWORK_METRICS_PROVIDER_H_
