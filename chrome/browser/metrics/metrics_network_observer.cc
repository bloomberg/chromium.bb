// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_network_observer.h"

#include "base/compiler_specific.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"


MetricsNetworkObserver::MetricsNetworkObserver()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      connection_type_is_ambiguous_(false),
      wifi_phy_layer_protocol_is_ambiguous_(false),
      wifi_phy_layer_protocol_(net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  ProbeWifiPHYLayerProtocol();
}

MetricsNetworkObserver::~MetricsNetworkObserver() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void MetricsNetworkObserver::Reset() {
  connection_type_is_ambiguous_ = false;
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  wifi_phy_layer_protocol_is_ambiguous_ = false;
}

void MetricsNetworkObserver::OnConnectionTypeChanged(
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
MetricsNetworkObserver::connection_type() const {
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
  }
  NOTREACHED();
  return SystemProfileProto::Network::CONNECTION_UNKNOWN;
}

SystemProfileProto::Network::WifiPHYLayerProtocol
MetricsNetworkObserver::wifi_phy_layer_protocol() const {
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

void MetricsNetworkObserver::ProbeWifiPHYLayerProtocol() {
  PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&net::GetWifiPHYLayerProtocol),
      base::Bind(&MetricsNetworkObserver::OnWifiPHYLayerProtocolResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MetricsNetworkObserver::OnWifiPHYLayerProtocolResult(
    net::WifiPHYLayerProtocol mode) {
  if (wifi_phy_layer_protocol_ != net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN &&
      mode != wifi_phy_layer_protocol_) {
    wifi_phy_layer_protocol_is_ambiguous_ = true;
  }
  wifi_phy_layer_protocol_ = mode;
}

