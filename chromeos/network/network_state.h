// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/network/managed_state.h"
#include "components/onc/onc_constants.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

// Simple class to provide network state information about a network service.
// This class should always be passed as a const* and should never be held
// on to. Store network_state->path() (defined in ManagedState) instead and
// call NetworkStateHandler::GetNetworkState(path) to retrieve the state for
// the network.
//
// Note: NetworkStateHandler will store an entry for each member of
// Manager.ServiceCompleteList. The visible() method indicates whether the
// network is visible, and the IsInProfile() method indicates whether the
// network is saved in a profile.
class CHROMEOS_EXPORT NetworkState : public ManagedState {
 public:
  explicit NetworkState(const std::string& path);
  virtual ~NetworkState();

  // ManagedState overrides
  // If you change this method, update GetProperties too.
  virtual bool PropertyChanged(const std::string& key,
                               const base::Value& value) OVERRIDE;
  virtual bool InitialPropertiesReceived(
      const base::DictionaryValue& properties) OVERRIDE;
  virtual void GetStateProperties(
      base::DictionaryValue* dictionary) const OVERRIDE;

  void IPConfigPropertiesChanged(const base::DictionaryValue& properties);

  // Returns true, if the network requires a service activation.
  bool RequiresActivation() const;

  // Accessors
  bool visible() const { return visible_; }
  const std::string& security() const { return security_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& guid() const { return guid_; }
  const std::string& profile_path() const { return profile_path_; }
  const std::string& error() const { return error_; }
  const std::string& last_error() const { return last_error_; }
  void clear_last_error() { last_error_.clear(); }

  // Returns |connection_state_| if visible, kStateDisconnect otherwise.
  std::string connection_state() const;

  const base::DictionaryValue& proxy_config() const { return proxy_config_; }

  // IPConfig Properties. These require an extra call to ShillIPConfigClient,
  // so cache them to avoid excessively complex client code.
  const std::string& ip_address() const { return ip_address_; }
  const std::string& gateway() const { return gateway_; }
  const std::vector<std::string>& dns_servers() const { return dns_servers_; }
  const GURL& web_proxy_auto_discovery_url() const {
    return web_proxy_auto_discovery_url_;
  }

  // Wireless property accessors
  bool connectable() const { return connectable_; }
  int signal_strength() const { return signal_strength_; }

  // Wifi property accessors
  const std::string& eap_method() const { return eap_method_; }

  // Cellular property accessors
  const std::string& network_technology() const {
    return network_technology_;
  }
  const std::string& activation_type() const { return activation_type_; }
  const std::string& activation_state() const { return activation_state_; }
  const std::string& roaming() const { return roaming_; }
  bool cellular_out_of_credits() const { return cellular_out_of_credits_; }

  // Whether this network has a CACertNSS nickname set.
  bool HasCACertNSS() const { return has_ca_cert_nss_; }

  // Returns true if |connection_state_| is a connected/connecting state.
  bool IsConnectedState() const;
  bool IsConnectingState() const;

  // Returns true if this is a network stored in a profile.
  bool IsInProfile() const;

  // Returns true if the network properties are stored in a user profile.
  bool IsPrivate() const;

  // Returns a comma separated string of name servers.
  std::string GetDnsServersAsString() const;

  // Converts the prefix length to a netmask string.
  std::string GetNetmask() const;

  // Returns a specifier for identifying this network in the absence of a GUID.
  // This should only be used by NetworkStateHandler for keeping track of
  // GUIDs assigned to unsaved networks.
  std::string GetSpecifier() const;

  // Set the GUID. Called exclusively by NetworkStateHandler.
  void SetGuid(const std::string& guid);

  // Helpers (used e.g. when a state or error is cached)
  static bool StateIsConnected(const std::string& connection_state);
  static bool StateIsConnecting(const std::string& connection_state);
  static bool ErrorIsValid(const std::string& error);

 private:
  friend class MobileActivatorTest;
  friend class NetworkStateHandler;
  friend class NetworkChangeNotifierChromeosUpdateTest;

  // Updates |name_| from WiFi.HexSSID if provided, and validates |name_|.
  // Returns true if |name_| changes.
  bool UpdateName(const base::DictionaryValue& properties);

  // Set to true if the network is a member of Manager.Services.
  bool visible_;

  // Network Service properties. Avoid adding any additional properties here.
  // Instead use NetworkConfigurationHandler::GetProperties() to asynchronously
  // request properties from Shill.
  std::string security_;
  std::string eap_method_;  // Needed for WiFi EAP networks
  std::string device_path_;
  std::string guid_;
  std::string connection_state_;
  std::string profile_path_;
  bool connectable_;

  // Reflects the current Shill Service.Error property. This might get cleared
  // by Shill shortly after a failure.
  std::string error_;

  // Last non empty Service.Error property. Cleared by NetworkConnectionHandler
  // when a connection attempt is initiated.
  std::string last_error_;

  // IPConfig properties.
  // Note: These do not correspond to actual Shill.Service properties
  // but are derived from the service's corresponding IPConfig object.
  std::string ip_address_;
  std::string gateway_;
  std::vector<std::string> dns_servers_;
  int prefix_length_;  // Used by GetNetmask()
  GURL web_proxy_auto_discovery_url_;

  // Wireless properties, used for icons and Connect logic.
  int signal_strength_;

  // Cellular properties, used for icons, Connect, and Activation.
  std::string network_technology_;
  std::string activation_type_;
  std::string activation_state_;
  std::string roaming_;
  bool cellular_out_of_credits_;

  // Whether a deprecated CaCertNSS property of this network is set. Required
  // for migration to PEM.
  bool has_ca_cert_nss_;

  // TODO(pneubeck): Remove this once (Managed)NetworkConfigurationHandler
  // provides proxy configuration. crbug.com/241775
  base::DictionaryValue proxy_config_;

  DISALLOW_COPY_AND_ASSIGN(NetworkState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_H_
