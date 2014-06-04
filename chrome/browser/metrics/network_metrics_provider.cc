// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_metrics_provider.h"

#include "base/compiler_specific.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

using metrics::SystemProfileProto;

NetworkMetricsProvider::NetworkMetricsProvider()
    : connection_type_is_ambiguous_(false),
      wifi_phy_layer_protocol_is_ambiguous_(false),
      wifi_phy_layer_protocol_(net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN),
      weak_ptr_factory_(this) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  ProbeWifiPHYLayerProtocol();
}

NetworkMetricsProvider::~NetworkMetricsProvider() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  SystemProfileProto::Network* network = system_profile->mutable_network();
  network->set_connection_type_is_ambiguous(connection_type_is_ambiguous_);
  network->set_connection_type(GetConnectionType());
  network->set_wifi_phy_layer_protocol_is_ambiguous(
      wifi_phy_layer_protocol_is_ambiguous_);
  network->set_wifi_phy_layer_protocol(GetWifiPHYLayerProtocol());

  // Resets the "ambiguous" flags, since a new metrics log session has started.
  connection_type_is_ambiguous_ = false;
  // TODO(isherman): This line seems unnecessary.
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  wifi_phy_layer_protocol_is_ambiguous_ = false;
}

void NetworkMetricsProvider::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;
  if (type != connection_type_ &&
      connection_type_ != net::NetworkChangeNotifier::CONNECTION_NONE) {
    connection_type_is_ambiguous_ = true;
  }
  connection_type_ = type;

  ProbeWifiPHYLayerProtocol();
}

SystemProfileProto::Network::ConnectionType
NetworkMetricsProvider::GetConnectionType() const {
  switch (connection_type_) {
    case net::NetworkChangeNotifier::CONNECTION_NONE:
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return SystemProfileProto::Network::CONNECTION_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return SystemProfileProto::Network::CONNECTION_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return SystemProfileProto::Network::CONNECTION_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return SystemProfileProto::Network::CONNECTION_2G;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return SystemProfileProto::Network::CONNECTION_3G;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return SystemProfileProto::Network::CONNECTION_4G;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return SystemProfileProto::Network::CONNECTION_BLUETOOTH;
  }
  NOTREACHED();
  return SystemProfileProto::Network::CONNECTION_UNKNOWN;
}

SystemProfileProto::Network::WifiPHYLayerProtocol
NetworkMetricsProvider::GetWifiPHYLayerProtocol() const {
  switch (wifi_phy_layer_protocol_) {
    case net::WIFI_PHY_LAYER_PROTOCOL_NONE:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_NONE;
    case net::WIFI_PHY_LAYER_PROTOCOL_ANCIENT:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case net::WIFI_PHY_LAYER_PROTOCOL_A:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_A;
    case net::WIFI_PHY_LAYER_PROTOCOL_B:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_B;
    case net::WIFI_PHY_LAYER_PROTOCOL_G:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_G;
    case net::WIFI_PHY_LAYER_PROTOCOL_N:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_N;
    case net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  }
  NOTREACHED();
  return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

void NetworkMetricsProvider::ProbeWifiPHYLayerProtocol() {
  PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&net::GetWifiPHYLayerProtocol),
      base::Bind(&NetworkMetricsProvider::OnWifiPHYLayerProtocolResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkMetricsProvider::OnWifiPHYLayerProtocolResult(
    net::WifiPHYLayerProtocol mode) {
  if (wifi_phy_layer_protocol_ != net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN &&
      mode != wifi_phy_layer_protocol_) {
    wifi_phy_layer_protocol_is_ambiguous_ = true;
  }
  wifi_phy_layer_protocol_ = mode;
}
