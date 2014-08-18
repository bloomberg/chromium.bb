// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_metrics_provider.h"

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/wifi_access_point_info_provider_chromeos.h"
#endif // OS_CHROMEOS

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

  if (!wifi_access_point_info_provider_.get()) {
#if defined(OS_CHROMEOS)
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProviderChromeos());
#else
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProvider());
#endif // OS_CHROMEOS
  }

  // Connected wifi access point information.
  WifiAccessPointInfoProvider::WifiAccessPointInfo info;
  if (wifi_access_point_info_provider_->GetInfo(&info))
    WriteWifiAccessPointProto(info, network);
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

void NetworkMetricsProvider::WriteWifiAccessPointProto(
    const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
    SystemProfileProto::Network* network_proto) {
  SystemProfileProto::Network::WifiAccessPoint* access_point_info =
      network_proto->mutable_access_point_info();
  SystemProfileProto::Network::WifiAccessPoint::SecurityMode security =
      SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
  switch (info.security) {
    case WifiAccessPointInfoProvider::WIFI_SECURITY_NONE:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_NONE;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WPA:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WPA;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WEP:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WEP;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_RSN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_RSN;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_802_1X:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_802_1X;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_PSK:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_PSK;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_UNKNOWN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
      break;
  }
  access_point_info->set_security_mode(security);

  // |bssid| is xx:xx:xx:xx:xx:xx, extract the first three components and
  // pack into a uint32.
  std::string bssid = info.bssid;
  if (bssid.size() == 17 && bssid[2] == ':' && bssid[5] == ':' &&
      bssid[8] == ':' && bssid[11] == ':' && bssid[14] == ':') {
    std::string vendor_prefix_str;
    uint32 vendor_prefix;

    base::RemoveChars(bssid.substr(0, 9), ":", &vendor_prefix_str);
    DCHECK_EQ(6U, vendor_prefix_str.size());
    if (base::HexStringToUInt(vendor_prefix_str, &vendor_prefix))
      access_point_info->set_vendor_prefix(vendor_prefix);
    else
      NOTREACHED();
  }

  // Return if vendor information is not provided.
  if (info.model_number.empty() && info.model_name.empty() &&
      info.device_name.empty() && info.oui_list.empty())
    return;

  SystemProfileProto::Network::WifiAccessPoint::VendorInformation* vendor =
      access_point_info->mutable_vendor_info();
  if (!info.model_number.empty())
    vendor->set_model_number(info.model_number);
  if (!info.model_name.empty())
    vendor->set_model_name(info.model_name);
  if (!info.device_name.empty())
    vendor->set_device_name(info.device_name);

  // Return if OUI list is not provided.
  if (info.oui_list.empty())
    return;

  // Parse OUI list.
  std::vector<std::string> oui_list;
  base::SplitString(info.oui_list, ' ', &oui_list);
  for (std::vector<std::string>::const_iterator it = oui_list.begin();
       it != oui_list.end();
       ++it) {
    uint32 oui;
    if (base::HexStringToUInt(*it, &oui))
      vendor->add_element_identifier(oui);
    else
      NOTREACHED();
  }
}
