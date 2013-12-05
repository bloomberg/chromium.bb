// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi/wifi_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "components/onc/onc_constants.h"

namespace wifi {

// Fake implementation of WiFiService used to satisfy expectations of
// networkingPrivateApi browser test.
class FakeWiFiService : public WiFiService {
 public:
  FakeWiFiService() {
    // Populate data expected by unit test.
    {
      WiFiService::NetworkProperties network_properties;
      network_properties.connection_state = onc::connection_state::kConnected;
      network_properties.guid = "stub_ethernet";
      network_properties.name = "eth0";
      network_properties.type = onc::network_type::kEthernet;
      network_properties.json_extra =
          "    {"
          "      \"Authentication\": \"None\""
          "    }";
      networks_.push_back(network_properties);
    }
    {
      WiFiService::NetworkProperties network_properties;
      network_properties.connection_state = onc::connection_state::kConnected;
      network_properties.guid = "stub_wifi1";
      network_properties.name = "wifi1";
      network_properties.type = onc::network_type::kWiFi;
      network_properties.frequency = 0;
      network_properties.ssid = "stub_wifi1";
      network_properties.security = onc::wifi::kWEP_PSK;
      network_properties.signal_strength = 0;
      networks_.push_back(network_properties);
    }
    {
      WiFiService::NetworkProperties network_properties;
      network_properties.connection_state = onc::connection_state::kConnected;
      network_properties.guid = "stub_vpn1";
      network_properties.name = "vpn1";
      network_properties.type = onc::network_type::kVPN;
      networks_.push_back(network_properties);
    }
    {
      WiFiService::NetworkProperties network_properties;
      network_properties.connection_state =
          onc::connection_state::kNotConnected;
      network_properties.guid = "stub_wifi2";
      network_properties.name = "wifi2_PSK";
      network_properties.type = onc::network_type::kWiFi;
      network_properties.frequency = 5000;
      network_properties.frequency_list.push_back(2400);
      network_properties.frequency_list.push_back(5000);
      network_properties.ssid = "wifi2_PSK";
      network_properties.security = onc::wifi::kWPA_PSK;
      network_properties.signal_strength = 80;
      networks_.push_back(network_properties);
    }
    {
      WiFiService::NetworkProperties network_properties;
      network_properties.connection_state =
          onc::connection_state::kNotConnected;
      network_properties.guid = "stub_cellular1";
      network_properties.name = "cellular1";
      network_properties.type = onc::network_type::kCellular;
      network_properties.json_extra =
          "    {"
          "      \"ActivateOverNonCellularNetwork\": false,"
          "      \"ActivationState\": \"not-activated\","
          "      \"NetworkTechnology\": \"GSM\","
          "      \"RoamingState\": \"home\""
          "    }";
      networks_.push_back(network_properties);
    }
  }

  virtual void Initialize(
    scoped_refptr<base::SequencedTaskRunner> task_runner) OVERRIDE {}

  virtual void UnInitialize() OVERRIDE {}

  virtual void GetProperties(const std::string& network_guid,
                              DictionaryValue* properties,
                              std::string* error) OVERRIDE {
    NetworkList::iterator network_properties = FindNetwork(network_guid);
    if (network_properties != networks_.end()) {
      properties->Swap(network_properties->ToValue(false).get());
    } else {
      *error = "Error.DBusFailed";
    }
  }

  virtual void GetManagedProperties(const std::string& network_guid,
                                    DictionaryValue* managed_properties,
                                    std::string* error) OVERRIDE {
    const std::string network_properties =
        "{"
        "  \"ConnectionState\": {"
        "    \"Active\": \"NotConnected\","
        "    \"Effective\": \"Unmanaged\""
        "  },"
        "  \"GUID\": \"stub_wifi2\","
        "  \"Name\": {"
        "    \"Active\": \"wifi2_PSK\","
        "    \"Effective\": \"UserPolicy\","
        "    \"UserPolicy\": \"My WiFi Network\""
        "  },"
        "  \"Type\": {"
        "    \"Active\": \"WiFi\","
        "    \"Effective\": \"UserPolicy\","
        "    \"UserPolicy\": \"WiFi\""
        "  },"
        "  \"WiFi\": {"
        "    \"AutoConnect\": {"
        "      \"Active\": false,"
        "      \"UserEditable\": true"
        "    },"
        "    \"Frequency\" : {"
        "      \"Active\": 5000,"
        "      \"Effective\": \"Unmanaged\""
        "    },"
        "    \"FrequencyList\" : {"
        "      \"Active\": [2400, 5000],"
        "      \"Effective\": \"Unmanaged\""
        "    },"
        "    \"Passphrase\": {"
        "      \"Effective\": \"UserSetting\","
        "      \"UserEditable\": true,"
        "      \"UserSetting\": \"FAKE_CREDENTIAL_VPaJDV9x\""
        "    },"
        "    \"SSID\": {"
        "      \"Active\": \"wifi2_PSK\","
        "      \"Effective\": \"UserPolicy\","
        "      \"UserPolicy\": \"wifi2_PSK\""
        "    },"
        "    \"Security\": {"
        "      \"Active\": \"WPA-PSK\","
        "      \"Effective\": \"UserPolicy\","
        "      \"UserPolicy\": \"WPA-PSK\""
        "    },"
        "    \"SignalStrength\": {"
        "      \"Active\": 80,"
        "      \"Effective\": \"Unmanaged\""
        "    }"
        "  }"
        "}";
    scoped_ptr<DictionaryValue> properties_value(
        reinterpret_cast<DictionaryValue*>(
            base::JSONReader::Read(network_properties)));
    managed_properties->MergeDictionary(properties_value.get());
  }

  virtual void GetState(const std::string& network_guid,
                        DictionaryValue* properties,
                        std::string* error) OVERRIDE {
    NetworkList::iterator network_properties = FindNetwork(network_guid);
    if (network_properties == networks_.end()) {
      *error = "Error.InvalidParameter";
      return;
    }

    const std::string network_state =
        "{"
        "  \"ConnectionState\": \"NotConnected\","
        "  \"GUID\": \"stub_wifi2\","
        "  \"Name\": \"wifi2_PSK\","
        "  \"Type\": \"WiFi\","
        "  \"WiFi\": {"
        "    \"Security\": \"WPA-PSK\","
        "    \"SignalStrength\": 80"
        "  }"
        "}";
    scoped_ptr<DictionaryValue> properties_value(
        reinterpret_cast<DictionaryValue*>(
            base::JSONReader::Read(network_state)));
    properties->MergeDictionary(properties_value.get());
  }

  virtual void SetProperties(const std::string& network_guid,
                             scoped_ptr<base::DictionaryValue> properties,
                             std::string* error) OVERRIDE {
    NetworkList::iterator network_properties = FindNetwork(network_guid);
    if (network_properties == networks_.end() ||
        !network_properties->UpdateFromValue(*properties)) {
      *error = "Error.DBusFailed";
    }
  }

  virtual void CreateNetwork(bool shared,
                             scoped_ptr<base::DictionaryValue> properties,
                             std::string* network_guid,
                             std::string* error) OVERRIDE {
    WiFiService::NetworkProperties network_properties;
    if (network_properties.UpdateFromValue(*properties)) {
      network_properties.guid = network_properties.ssid;
      networks_.push_back(network_properties);
      *network_guid = network_properties.guid;
    } else {
      *error = "Error.DBusFailed";
    }
  }

  virtual void GetVisibleNetworks(const std::string& network_type,
                                  ListValue* network_list) OVERRIDE {
    for (WiFiService::NetworkList::const_iterator it = networks_.begin();
         it != networks_.end();
         ++it) {
      if (network_type.empty() ||
          network_type == onc::network_type::kAllTypes ||
          it->type == network_type) {
        scoped_ptr<DictionaryValue> network(it->ToValue(true));
        network_list->Append(network.release());
      }
    }
  }

  virtual void RequestNetworkScan() OVERRIDE {
    NotifyNetworkListChanged(networks_);
  }

  virtual void StartConnect(const std::string& network_guid,
                            std::string* error) OVERRIDE {
    NetworkList::iterator network_properties = FindNetwork(network_guid);
    if (network_properties != networks_.end()) {
      DisconnectAllNetworksOfType(network_properties->type);
      network_properties->connection_state = onc::connection_state::kConnected;
      SortNetworks();
      NotifyNetworkListChanged(networks_);
      NotifyNetworkChanged(network_guid);
    } else {
      *error = "configure-failed";
    }
  }

  virtual void StartDisconnect(const std::string& network_guid,
                               std::string* error) OVERRIDE {
    NetworkList::iterator network_properties = FindNetwork(network_guid);
    if (network_properties != networks_.end()) {
      network_properties->connection_state =
          onc::connection_state::kNotConnected;
      SortNetworks();
      NotifyNetworkListChanged(networks_);
      NotifyNetworkChanged(network_guid);
    } else {
      *error = "not-found";
    }
  }

  virtual void SetEventObservers(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const NetworkGuidListCallback& networks_changed_observer,
      const NetworkGuidListCallback& network_list_changed_observer) OVERRIDE {
    message_loop_proxy_.swap(message_loop_proxy);
    networks_changed_observer_ = networks_changed_observer;
    network_list_changed_observer_ = network_list_changed_observer;
  }

 private:
  NetworkList::iterator FindNetwork(const std::string& network_guid) {
    for (NetworkList::iterator it = networks_.begin(); it != networks_.end();
         ++it) {
      if (it->guid == network_guid)
        return it;
    }
    return networks_.end();
  }

  void DisconnectAllNetworksOfType(const std::string& type) {
    for (NetworkList::iterator it = networks_.begin(); it != networks_.end();
         ++it) {
      if (it->type == type)
        it->connection_state = onc::connection_state::kNotConnected;
    }
  }

  void SortNetworks() {
    // Sort networks, so connected/connecting is up front, then by type:
    // Ethernet, WiFi, Cellular, VPN
    networks_.sort(WiFiService::NetworkProperties::OrderByType);
  }

  void NotifyNetworkListChanged(const NetworkList& networks) {
    WiFiService::NetworkGuidList current_networks;
    for (WiFiService::NetworkList::const_iterator it = networks.begin();
         it != networks.end();
         ++it) {
      current_networks.push_back(it->guid);
    }

    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(network_list_changed_observer_, current_networks));
  }

  void NotifyNetworkChanged(const std::string& network_guid) {
    WiFiService::NetworkGuidList changed_networks(1, network_guid);
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(networks_changed_observer_, changed_networks));
  }

  NetworkList networks_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  NetworkGuidListCallback networks_changed_observer_;
  NetworkGuidListCallback network_list_changed_observer_;
};

WiFiService* WiFiService::CreateForTest() { return new FakeWiFiService(); }

}  // namespace wifi
