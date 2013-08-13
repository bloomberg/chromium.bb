// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_constants.h"
#include "url/gurl.h"

namespace chromeos {

// Simple class to provide network state information about a network service.
// This class should always be passed as a const* and should never be held
// on to. Store network_state->path() (defined in ManagedState) instead and
// call NetworkStateHandler::GetNetworkState(path) to retrieve the state for
// the network.
class CHROMEOS_EXPORT NetworkState : public ManagedState {
 public:
  typedef std::vector<int> FrequencyList;

  explicit NetworkState(const std::string& path);
  virtual ~NetworkState();

  // ManagedState overrides
  // If you change this method, update GetProperties too.
  virtual bool PropertyChanged(const std::string& key,
                               const base::Value& value) OVERRIDE;
  virtual bool InitialPropertiesReceived(
      const base::DictionaryValue& properties) OVERRIDE;

  // Fills |dictionary| with the state properties. All the properties that are
  // accepted by PropertyChanged are stored in |dictionary|, no other values are
  // stored.
  void GetProperties(base::DictionaryValue* dictionary) const;

  // Accessors
  const std::string& security() const { return security_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& guid() const { return guid_; }
  const std::string& connection_state() const { return connection_state_; }
  const std::string& profile_path() const { return profile_path_; }
  const std::string& error() const { return error_; }
  const std::string& error_details() const { return error_details_; }
  bool auto_connect() const { return auto_connect_; }
  bool favorite() const { return favorite_; }
  int priority() const { return priority_; }
  const base::DictionaryValue& proxy_config() const { return proxy_config_; }
  const NetworkUIData& ui_data() const { return ui_data_; }
  // IPConfig Properties
  const std::string& ip_address() const { return ip_address_; }
  const std::string& gateway() const { return gateway_; }
  const std::vector<std::string>& dns_servers() const { return dns_servers_; }
  const int prefix_length() const { return prefix_length_; }
  const GURL& web_proxy_auto_discovery_url() const {
    return web_proxy_auto_discovery_url_;
  }
  // Wireless property accessors
  int signal_strength() const { return signal_strength_; }
  bool connectable() const { return connectable_; }
  // Cellular property accessors
  const std::string& network_technology() const {
    return network_technology_;
  }
  const std::string& activation_state() const { return activation_state_; }
  const std::string& roaming() const { return roaming_; }
  bool activate_over_non_cellular_networks() const {
    return activate_over_non_cellular_networks_;
  }
  bool cellular_out_of_credits() const { return cellular_out_of_credits_; }
  const std::string& usage_url() const { return usage_url_; }
  const std::string& payment_url() const { return payment_url_; }
  const std::string& post_method() const { return post_method_; }
  const std::string& post_data() const { return post_data_; }

  // Whether this network has a CACertNSS nickname set.
  bool HasCACertNSS() const { return has_ca_cert_nss_; }

  // Returns true if |connection_state_| is a connected/connecting state.
  bool IsConnectedState() const;
  bool IsConnectingState() const;

  // Returns true if the ONC source is a device or user policy.
  bool IsManaged() const;

  // Returns true if the network properties are stored in a user profile.
  bool IsPrivate() const;

  // Returns a comma separated string of name servers.
  std::string GetDnsServersAsString() const;

  // Converts the prefix length to a netmask string.
  std::string GetNetmask() const;

  // Helpers (used e.g. when a state is cached)
  static bool StateIsConnected(const std::string& connection_state);
  static bool StateIsConnecting(const std::string& connection_state);

  // Helper to return a full prefixed version of an IPConfig property
  // key.
  static std::string IPConfigProperty(const char* key);

  // Sets |out| to the UIData specified by |value|. Returns true if successfully
  // parsed.
  static bool GetUIDataFromValue(const base::Value& value, NetworkUIData* out);

  // Generates a name from properties."Wifi.HexSSID" if present, otherwise
  // validates properties.Name and returns a valid utf8 version.
  static std::string GetNameFromProperties(
      const std::string& service_path,
      const base::DictionaryValue& properties);

 private:
  friend class NetworkStateHandler;
  friend class NetworkChangeNotifierChromeosUpdateTest;

  // Updates |name_| from WiFi.HexSSID if provided, and validates |name_|.
  // Returns true if |name_| changes.
  bool UpdateName(const base::DictionaryValue& properties);

  // TODO(gauravsh): Audit the list of properties that we are caching. We should
  // only be doing this for commonly accessed properties. crbug.com/252553
  // Common Network Service properties
  std::string security_;
  std::string device_path_;
  std::string guid_;
  std::string connection_state_;
  std::string profile_path_;
  std::string error_;
  std::string error_details_;
  bool auto_connect_;
  bool favorite_;
  int priority_;
  // TODO(pneubeck): Remove ProxyConfig once NetworkConfigurationHandler
  // provides proxy configuration. crbug/241775
  base::DictionaryValue proxy_config_;
  NetworkUIData ui_data_;
  // IPConfig properties.
  // Note: These do not correspond to actual Shill.Service properties
  // but are derived from the service's corresponding IPConfig object.
  std::string ip_address_;
  std::string gateway_;
  std::vector<std::string> dns_servers_;
  int prefix_length_;
  GURL web_proxy_auto_discovery_url_;
  // Wireless properties
  int signal_strength_;
  bool connectable_;
  // Cellular properties
  std::string network_technology_;
  std::string activation_state_;
  std::string roaming_;
  bool activate_over_non_cellular_networks_;
  bool cellular_out_of_credits_;
  // Cellular payment portal properties.
  std::string usage_url_;
  std::string payment_url_;
  std::string post_method_;
  std::string post_data_;

  // Whether a deprecated CaCertNSS property of this network is set. Required
  // for migration to PEM.
  bool has_ca_cert_nss_;

  DISALLOW_COPY_AND_ASSIGN(NetworkState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_H_
