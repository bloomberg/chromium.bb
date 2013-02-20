// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_NETWORK_OBSERVER_H_
#define CHROME_BROWSER_METRICS_METRICS_NETWORK_OBSERVER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"

using metrics::SystemProfileProto;

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class MetricsNetworkObserver
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  MetricsNetworkObserver();
  virtual ~MetricsNetworkObserver();

  // Resets the "ambiguous" flags. Call when the environment is recorded.
  void Reset();

  // ConnectionTypeObserver:
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  bool connection_type_is_ambiguous() const {
    return connection_type_is_ambiguous_;
  }

  SystemProfileProto::Network::ConnectionType connection_type() const;

  bool wifi_phy_layer_protocol_is_ambiguous() const {
    return wifi_phy_layer_protocol_is_ambiguous_;
  }

  SystemProfileProto::Network::WifiPHYLayerProtocol
  wifi_phy_layer_protocol() const;

 private:
  // Posts a call to net::GetWifiPHYLayerProtocol on the blocking pool.
  void ProbeWifiPHYLayerProtocol();
  // Callback from the blocking pool with the result of
  // net::GetWifiPHYLayerProtocol.
  void OnWifiPHYLayerProtocolResult(net::WifiPHYLayerProtocol mode);

  base::WeakPtrFactory<MetricsNetworkObserver> weak_ptr_factory_;

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to net::NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // True if |wifi_phy_layer_protocol_| changed during the lifetime of the log.
  bool wifi_phy_layer_protocol_is_ambiguous_;
  // The PHY mode of the currently associated access point obtained via
  // net::GetWifiPHYLayerProtocol.
  net::WifiPHYLayerProtocol wifi_phy_layer_protocol_;

  DISALLOW_COPY_AND_ASSIGN(MetricsNetworkObserver);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_NETWORK_OBSERVER_H_
