// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/wifi_hotspot_connector.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "components/proximity_auth/logging/logging.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

WifiHotspotConnector::WifiHotspotConnector(
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect)
    : network_state_handler_(network_state_handler),
      network_connect_(network_connect),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      weak_ptr_factory_(this) {
  network_state_handler_->AddObserver(this, FROM_HERE);
}

WifiHotspotConnector::~WifiHotspotConnector() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void WifiHotspotConnector::ConnectToWifiHotspot(
    const std::string& ssid,
    const std::string& password,
    const std::string& tether_network_guid,
    const WifiConnectionCallback& callback) {
  DCHECK(!ssid.empty());
  // Note: |password| can be empty in some cases.

  if (!callback_.is_null()) {
    DCHECK(timer_->IsRunning());

    // If another connection attempt was underway but had not yet completed,
    // disassociate that network from the Tether network and call the callback,
    // passing an empty string to signal that the connection did not complete
    // successfully.
    bool successful_disassociation =
        network_state_handler_->DisassociateTetherNetworkStateFromWifiNetwork(
            tether_network_guid_);
    if (successful_disassociation) {
      PA_LOG(INFO) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                   << "successfully disassociated from Tether network (ID "
                   << "\"" << tether_network_guid_ << "\").";
    } else {
      PA_LOG(INFO) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                   << "failed to disassociate from Tether network ID (\""
                   << tether_network_guid_ << "\").";
    }

    InvokeWifiConnectionCallback(std::string());
  }

  ssid_ = ssid;
  password_ = password;
  tether_network_guid_ = tether_network_guid;
  wifi_network_guid_ = base::GenerateGUID();
  callback_ = callback;
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kConnectionTimeoutSeconds),
                base::Bind(&WifiHotspotConnector::OnConnectionTimeout,
                           weak_ptr_factory_.GetWeakPtr()));

  // If Wi-Fi is enabled, continue with creating the configuration of the
  // hotspot. Otherwise, request that Wi-Fi be enabled and wait; see
  // DeviceListChanged().
  if (network_state_handler_->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    // Ensure that a possible previous pending callback to DeviceListChanged()
    // won't result in a second call to CreateWifiConfiguration().
    is_waiting_for_wifi_to_enable_ = false;

    CreateWifiConfiguration();
  } else if (!is_waiting_for_wifi_to_enable_) {
    is_waiting_for_wifi_to_enable_ = true;

    // Once Wi-Fi is enabled, DeviceListChanged() will be called.
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::WiFi(), true /*enabled */,
        base::Bind(&WifiHotspotConnector::OnEnableWifiError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void WifiHotspotConnector::OnEnableWifiError(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  is_waiting_for_wifi_to_enable_ = false;
  PA_LOG(ERROR) << "Failed to enable Wi-Fi: " << error_name;
}

void WifiHotspotConnector::DeviceListChanged() {
  if (is_waiting_for_wifi_to_enable_ &&
      network_state_handler_->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    is_waiting_for_wifi_to_enable_ = false;

    if (!ssid_.empty())
      CreateWifiConfiguration();
  }
}

void WifiHotspotConnector::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (network->guid() != wifi_network_guid_) {
    // If a different network has been connected, return early and wait for the
    // network with ID |wifi_network_guid_| is updated.
    return;
  }

  if (network->IsConnectedState()) {
    // If a connection occurred, notify observers and exit early.
    InvokeWifiConnectionCallback(wifi_network_guid_);
    return;
  }

  if (network->connectable() && !has_initiated_connection_to_current_network_) {
    // Set |has_initiated_connection_to_current_network_| to true to ensure that
    // this code path is only run once per connection attempt. Without this
    // field, the association and connection code below would be re-run multiple
    // times for one network.
    has_initiated_connection_to_current_network_ = true;

    // If the network is now connectable, associate it with a Tether network
    // ASAP so that the correct icon will be displayed in the tray while the
    // network is connecting.
    bool successful_association =
        network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
            tether_network_guid_, wifi_network_guid_);
    if (successful_association) {
      PA_LOG(INFO) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                   << "successfully associated with Tether network (ID \""
                   << tether_network_guid_ << "\"). Starting connection "
                   << "attempt.";
    } else {
      PA_LOG(INFO) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                   << "failed to associate with Tether network (ID \""
                   << tether_network_guid_ << "\"). Starting connection "
                   << "attempt.";
    }

    // Initiate a connection to the network.
    network_connect_->ConnectToNetworkId(wifi_network_guid_);
  }
}

void WifiHotspotConnector::InvokeWifiConnectionCallback(
    const std::string& wifi_guid) {
  DCHECK(!callback_.is_null());

  // |wifi_guid| may be a reference to |wifi_network_guid_|, so make a copy of
  // it first before clearing it below.
  std::string wifi_network_guid_copy = wifi_guid;

  ssid_.clear();
  password_.clear();
  wifi_network_guid_.clear();
  has_initiated_connection_to_current_network_ = false;
  is_waiting_for_wifi_to_enable_ = false;

  timer_->Stop();

  callback_.Run(wifi_network_guid_copy);
  callback_.Reset();
}

void WifiHotspotConnector::CreateWifiConfiguration() {
  base::DictionaryValue properties =
      CreateWifiPropertyDictionary(ssid_, password_);

  // This newly configured network will eventually be passed as an argument to
  // NetworkPropertiesUpdated().
  network_connect_->CreateConfiguration(&properties, false /* shared */);
}

base::DictionaryValue WifiHotspotConnector::CreateWifiPropertyDictionary(
    const std::string& ssid,
    const std::string& password) {
  PA_LOG(INFO) << "Creating network configuration. "
               << "SSID: " << ssid << ", "
               << "Password: " << password << ", "
               << "Wi-Fi network GUID: " << wifi_network_guid_;

  base::DictionaryValue properties;

  shill_property_util::SetSSID(ssid, &properties);
  properties.SetKey(shill::kGuidProperty, base::Value(wifi_network_guid_));
  properties.SetKey(shill::kAutoConnectProperty, base::Value(false));
  properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  properties.SetKey(shill::kSaveCredentialsProperty, base::Value(true));

  if (password.empty()) {
    properties.SetKey(shill::kSecurityClassProperty,
                      base::Value(shill::kSecurityNone));
  } else {
    properties.SetKey(shill::kSecurityClassProperty,
                      base::Value(shill::kSecurityPsk));
    properties.SetKey(shill::kPassphraseProperty, base::Value(password));
  }

  return properties;
}

void WifiHotspotConnector::OnConnectionTimeout() {
  InvokeWifiConnectionCallback(std::string());
}

void WifiHotspotConnector::SetTimerForTest(std::unique_ptr<base::Timer> timer) {
  timer_ = std::move(timer);
}

}  // namespace tether

}  // namespace chromeos
