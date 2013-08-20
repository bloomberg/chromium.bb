// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/networking_private.h"

using extensions::EventRouter;
using extensions::ExtensionSystem;
namespace api = extensions::api::networking_private;

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction


const char kNetworkingPrivateProperties[] = "NetworkingPrivateProperties";

struct NetworkingPrivatePropertiesData : base::SupportsUserData::Data {
  explicit NetworkingPrivatePropertiesData(const base::DictionaryValue* prop) :
    properties_(prop->DeepCopy()) { }
  scoped_ptr<base::DictionaryValue> properties_;
};

NetworkingPrivateGetPropertiesFunction::
  ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetProperties::Params> params =
      api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // If there are properties set by SetProperties function, use those.
  NetworkingPrivatePropertiesData* stored_properties =
    static_cast<NetworkingPrivatePropertiesData*> (
        profile()->GetUserData(kNetworkingPrivateProperties));
  if (stored_properties != NULL) {
    SetResult(stored_properties->properties_.release());
    SendResponse(true);
    return true;
  }

  const std::string network_properties =
    "{\"ConnectionState\":\"NotConnected\","
     "\"GUID\":\"stub_wifi2\","
     "\"Name\":\"wifi2_PSK\","
     "\"Type\":\"WiFi\","
     "\"WiFi\":{"
       "\"Frequency\":5000,"
       "\"FrequencyList\":[2400,5000],"
       "\"SSID\":\"stub_wifi2\","
       "\"Security\":\"WPA-PSK\","
       "\"SignalStrength\":80}}";

  if (params->network_guid == "nonexistent_path") {
    error_ = "Error.DBusFailed";
    SendResponse(false);
  } else {
    SetResult(base::JSONReader::Read(network_properties));
    SendResponse(true);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

NetworkingPrivateGetManagedPropertiesFunction::
  ~NetworkingPrivateGetManagedPropertiesFunction() {
}

bool NetworkingPrivateGetManagedPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetManagedProperties::Params> params =
      api::GetManagedProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
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
      "      \"Active\": \"stub_wifi2\","
      "      \"Effective\": \"UserPolicy\","
      "      \"UserPolicy\": \"stub_wifi2\""
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

  SetResult(base::JSONReader::Read(network_properties));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

NetworkingPrivateGetStateFunction::
  ~NetworkingPrivateGetStateFunction() {
}

bool NetworkingPrivateGetStateFunction::RunImpl() {
  scoped_ptr<api::GetState::Params> params =
      api::GetState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string network_state =
      "{"
      "  \"ConnectionState\": \"NotConnected\","
      "  \"GUID\": \"stub_wifi2\","
      "  \"Name\": \"wifi2_PSK\","
      "  \"Type\": \"WiFi\","
      "  \"WiFi\": {"
      "    \"AutoConnect\": false,"
      "    \"Security\": \"WPA-PSK\","
      "    \"SignalStrength\": 80"
      "  }"
      "}";
  SetResult(base::JSONReader::Read(network_state));
  SendResponse(true);
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
  ~NetworkingPrivateSetPropertiesFunction() {
}

bool NetworkingPrivateSetPropertiesFunction::RunImpl() {
  scoped_ptr<api::SetProperties::Params> params =
      api::SetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  // Store properties_dict in profile to return from GetProperties.
  profile()->SetUserData(kNetworkingPrivateProperties,
    new NetworkingPrivatePropertiesData(properties_dict.get()));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
  ~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunImpl() {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string networks_json =
      "[{"
      "    \"ConnectionState\": \"Connected\","
      "    \"GUID\": \"stub_ethernet\","
      "    \"Name\": \"eth0\","
      "    \"Type\": \"Ethernet\""
      "  },"
      "  {"
      "    \"ConnectionState\": \"Connected\","
      "    \"GUID\": \"stub_wifi1\","
      "    \"Name\": \"wifi1\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"AutoConnect\": false,"
      "      \"Security\": \"WEP-PSK\","
      "      \"SignalStrength\": 0"
      "    }"
      "  },"
      "  {"
      "    \"ConnectionState\": \"Connected\","
      "    \"GUID\": \"stub_vpn1\","
      "    \"Name\": \"vpn1\","
      "    \"Type\": \"VPN\","
      "    \"VPN\": {"
      "      \"AutoConnect\": false"
      "    }"
      "  },"
      "  {"
      "    \"ConnectionState\": \"NotConnected\","
      "    \"GUID\": \"stub_wifi2\","
      "    \"Name\": \"wifi2_PSK\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"AutoConnect\": false,"
      "      \"Security\": \"WPA-PSK\","
      "      \"SignalStrength\": 80"
      "    }"
      "  },"
      "  {"
      "    \"Cellular\": {"
      "      \"ActivateOverNonCellularNetwork\": false,"
      "      \"ActivationState\": \"not-activated\","
      "      \"NetworkTechnology\": \"GSM\","
      "      \"RoamingState\": \"home\""
      "    },"
      "    \"ConnectionState\": \"NotConnected\","
      "    \"GUID\": \"stub_cellular1\","
      "    \"Name\": \"cellular1\","
      "    \"Type\": \"Cellular\""
      "  }]";
  ListValue* visible_networks =
      static_cast<ListValue*>(base::JSONReader::Read(networks_json));
  // If caller only needs WiFi networks, then remove all other networks.
  if (params->type == api::GetVisibleNetworks::Params::TYPE_WIFI) {
    visible_networks->Remove(4, NULL);
    visible_networks->Remove(2, NULL);
    visible_networks->Remove(0, NULL);
  }

  SetResult(visible_networks);
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
  ~NetworkingPrivateRequestNetworkScanFunction() {
}

bool NetworkingPrivateRequestNetworkScanFunction::RunImpl() {
  // Generate onNetworkListChanged event.
  std::vector<std::string> changes;
  changes.push_back("stub_ethernet");
  changes.push_back("stub_wifi1");
  changes.push_back("stub_vpn1");
  changes.push_back("stub_wifi2");
  changes.push_back("stub_cellular1");

  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  scoped_ptr<base::ListValue> args(api::OnNetworkListChanged::Create(changes));
  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      api::OnNetworkListChanged::kEventName, args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
  ~NetworkingPrivateStartConnectFunction() {
}

bool NetworkingPrivateStartConnectFunction::RunImpl() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->network_guid == "nonexistent_path") {
    error_ = "configure-failed";
    SendResponse(false);
  } else {
    SendResponse(true);
    // Set Properties to reflect connected state
    const std::string network_properties =
      "{\"ConnectionState\":\"Connected\","
       "\"GUID\":\"stub_wifi2\","
       "\"Name\":\"wifi2_PSK\","
       "\"Type\":\"WiFi\","
       "\"WiFi\":{"
         "\"SSID\":\"stub_wifi2\","
         "\"Security\":\"WPA-PSK\","
         "\"SignalStrength\":80}}";

    // Store network_properties in profile to return from GetProperties.
    profile()->SetUserData(kNetworkingPrivateProperties,
      new NetworkingPrivatePropertiesData(
        static_cast<DictionaryValue*>(
          base::JSONReader::Read(network_properties))));

    // Broadcast NetworksChanged Event that network is connected
    EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
    scoped_ptr<base::ListValue> args(api::OnNetworksChanged::Create(
        std::vector<std::string>(1, params->network_guid)));
    scoped_ptr<extensions::Event> netchanged_event(
        new extensions::Event(api::OnNetworksChanged::kEventName, args.Pass()));
    event_router->BroadcastEvent(netchanged_event.Pass());

    // Generate NetworkListChanged event.
    std::vector<std::string> list;
    list.push_back("stub_ethernet");
    list.push_back("stub_wifi2");
    list.push_back("stub_vpn1");
    list.push_back("stub_wifi1");
    list.push_back("stub_cellular1");

    scoped_ptr<base::ListValue> arg2(api::OnNetworkListChanged::Create(list));
    scoped_ptr<extensions::Event> netlist_event(new extensions::Event(
        api::OnNetworkListChanged::kEventName, arg2.Pass()));
    event_router->BroadcastEvent(netlist_event.Pass());
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
  ~NetworkingPrivateStartDisconnectFunction() {
}

bool NetworkingPrivateStartDisconnectFunction::RunImpl() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->network_guid == "nonexistent_path") {
    error_ = "not-found";
    SendResponse(false);
  } else {
    SendResponse(true);

    // Send Event that network is disconnected. Listener will use GetProperties.
    EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
    scoped_ptr<base::ListValue> args(api::OnNetworksChanged::Create(
        std::vector<std::string>(1, params->network_guid)));
    scoped_ptr<extensions::Event> extension_event(
        new extensions::Event(api::OnNetworksChanged::kEventName, args.Pass()));
    event_router->BroadcastEvent(extension_event.Pass());
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyDestinationFunction

NetworkingPrivateVerifyDestinationFunction::
  ~NetworkingPrivateVerifyDestinationFunction() {
}

bool NetworkingPrivateVerifyDestinationFunction::RunImpl() {
  scoped_ptr<api::VerifyDestination::Params> params =
      api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetResult(new base::FundamentalValue(true));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptCredentialsFunction

NetworkingPrivateVerifyAndEncryptCredentialsFunction::
  ~NetworkingPrivateVerifyAndEncryptCredentialsFunction() {
}

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptCredentials::Params> params =
      api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetResult(new base::StringValue("encrypted_credentials"));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptDataFunction

NetworkingPrivateVerifyAndEncryptDataFunction::
  ~NetworkingPrivateVerifyAndEncryptDataFunction() {
}

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptData::Params> params =
      api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetResult(new base::StringValue("encrypted_data"));
  SendResponse(true);
  return true;
}
