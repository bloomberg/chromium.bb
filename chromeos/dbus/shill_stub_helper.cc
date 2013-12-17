// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_stub_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_tokenizer.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace shill_stub_helper {

namespace {

const char kDevicePathEthernet[] = "/device/eth1";
const char kDevicePathWifi[] = "/device/wifi1";
const char kDevicePathCellular[] = "/device/cellular1";
const char kDevicePathWimax[] = "/device/wimax1";

const char kStubPortalledWifiName[] = "Portalled Wifi";
const char kStubPortalledWifiPath[] = "portalled_wifi";

void UpdatePortalledWifiState() {
  ShillServiceClient::TestInterface* services =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

  services->SetServiceProperty(kStubPortalledWifiPath,
                               shill::kStateProperty,
                               base::StringValue(shill::kStatePortal));
}

// Returns true if |network_type| is found in the comma separated list given
// with kEnabledStubNetworkTypes switch.
bool IsStubNetworkTypeEnabled(const std::string& network_type) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  // If the switch is not present, enabled by default.
  if (!command_line->HasSwitch(switches::kEnabledStubNetworkTypes))
    return true;

  const std::string value =
      command_line->GetSwitchValueASCII(switches::kEnabledStubNetworkTypes);
  base::StringTokenizer tokenizer(value, ",");
  while (tokenizer.GetNext()) {
    if (tokenizer.token() == network_type)
      return true;
  }
  return false;
}

}  // namespace

const char kSharedProfilePath[] = "/profile/default";

bool IsStubPortalledWifiEnabled(const std::string& path) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kEnableStubPortalledWifi)) {
    return false;
  }
  return path == kStubPortalledWifiPath;
}

void SetupDefaultEnvironment() {
  ShillServiceClient::TestInterface* services =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  ShillProfileClient::TestInterface* profiles =
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
  ShillManagerClient::TestInterface* manager =
      DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
  ShillDeviceClient::TestInterface* devices =
      DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
  if (!services || !profiles || !manager | !devices)
    return;

  // Stub Technologies.
  manager->AddTechnology(shill::kTypeEthernet, true);
  manager->AddTechnology(shill::kTypeWifi, true);
  manager->AddTechnology(shill::kTypeCellular, true);
  manager->AddTechnology(shill::kTypeWimax, true);

  profiles->AddProfile(kSharedProfilePath, std::string());

  devices->AddDevice(
      kDevicePathEthernet, shill::kTypeEthernet, "stub_eth_device1");
  devices->AddDevice(kDevicePathWifi, shill::kTypeWifi, "stub_wifi_device1");

  devices->AddDevice(
      kDevicePathCellular, shill::kTypeCellular, "stub_cellular_device1");
  devices->SetDeviceProperty(kDevicePathCellular,
                             shill::kCarrierProperty,
                             base::StringValue(shill::kCarrierSprint));

  devices->AddDevice(kDevicePathWimax, shill::kTypeWimax, "stub_wimax_device1");

  const bool add_to_visible = true;
  const bool add_to_watchlist = true;

  // On real devices, service is not added for ethernet if cable is disconneted.
  if (IsStubNetworkTypeEnabled(shill::kTypeEthernet)) {
    services->AddService("eth1", "eth1",
                         shill::kTypeEthernet,
                         shill::kStateOnline,
                         add_to_visible, add_to_watchlist);
    profiles->AddService(kSharedProfilePath, "eth1");
  }

  // Wifi

  services->AddService("wifi1",
                       "wifi1",
                       shill::kTypeWifi,
                       IsStubNetworkTypeEnabled(shill::kTypeWifi) ?
                           shill::kStateOnline : shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty("wifi1",
                               shill::kSecurityProperty,
                               base::StringValue(shill::kSecurityWep));
  profiles->AddService(kSharedProfilePath, "wifi1");

  services->AddService("wifi2",
                       "wifi2_PSK",
                       shill::kTypeWifi,
                       shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty("wifi2",
                               shill::kSecurityProperty,
                               base::StringValue(shill::kSecurityPsk));
  base::FundamentalValue strength_value(80);
  services->SetServiceProperty(
      "wifi2", shill::kSignalStrengthProperty, strength_value);
  profiles->AddService(kSharedProfilePath, "wifi2");

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableStubPortalledWifi)) {
    services->AddService(kStubPortalledWifiPath,
                         kStubPortalledWifiName,
                         shill::kTypeWifi,
                         shill::kStatePortal,
                         add_to_visible, add_to_watchlist);
    services->SetServiceProperty(kStubPortalledWifiPath,
                                 shill::kSecurityProperty,
                                 base::StringValue(shill::kSecurityNone));
    services->SetConnectBehavior(kStubPortalledWifiPath,
                                 base::Bind(&UpdatePortalledWifiState));
    services->SetServiceProperty(kStubPortalledWifiPath,
                                 shill::kConnectableProperty,
                                 base::FundamentalValue(true));
  }

  // Wimax

  services->AddService("wimax1",
                       "wimax1",
                       shill::kTypeWimax,
                       shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "wimax1", shill::kConnectableProperty, base::FundamentalValue(true));

  // Cellular

  services->AddService("cellular1",
                       "cellular1",
                       shill::kTypeCellular,
                       shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  base::StringValue technology_value(shill::kNetworkTechnologyGsm);
  services->SetServiceProperty(
      "cellular1", shill::kNetworkTechnologyProperty, technology_value);
  services->SetServiceProperty(
      "cellular1",
      shill::kActivationStateProperty,
      base::StringValue(shill::kActivationStateNotActivated));
  services->SetServiceProperty("cellular1",
                               shill::kRoamingStateProperty,
                               base::StringValue(shill::kRoamingStateHome));

  // VPN

  // Set the "Provider" dictionary properties. Note: when setting these in
  // Shill, "Provider.Type", etc keys are used, but when reading the values
  // "Provider" . "Type", etc keys are used. Here we are setting the values
  // that will be read (by the UI, tests, etc).
  base::DictionaryValue provider_properties;
  provider_properties.SetString(shill::kTypeProperty, shill::kProviderOpenVpn);
  provider_properties.SetString(shill::kHostProperty, "vpn_host");

  services->AddService("vpn1",
                       "vpn1",
                       shill::kTypeVPN,
                       IsStubNetworkTypeEnabled(shill::kTypeVPN) ?
                           shill::kStateOnline : shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "vpn1", shill::kProviderProperty, provider_properties);
  profiles->AddService(kSharedProfilePath, "vpn1");

  services->AddService("vpn2",
                       "vpn2",
                       shill::kTypeVPN,
                       shill::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "vpn2", shill::kProviderProperty, provider_properties);

  manager->SortManagerServices();
}

std::string DevicePathForType(const std::string& type) {
  if (type == shill::kTypeEthernet)
    return kDevicePathEthernet;
  if (type == shill::kTypeWifi)
    return kDevicePathWifi;
  if (type == shill::kTypeCellular)
    return kDevicePathCellular;
  if (type == shill::kTypeWimax)
    return kDevicePathWimax;
  return "";
}

}  // namespace shill_stub_helper
}  // namespace chromeos
