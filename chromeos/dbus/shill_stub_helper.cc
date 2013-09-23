// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_stub_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_profile_client_stub.h"
#include "chromeos/dbus/shill_service_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace shill_stub_helper {

namespace {

const char kStubPortalledWifiName[] = "Portalled Wifi";
const char kStubPortalledWifiPath[] = "portalled_wifi";

void UpdatePortalledWifiState() {
  ShillServiceClient::TestInterface* services =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

  services->SetServiceProperty(kStubPortalledWifiPath,
                               flimflam::kStateProperty,
                               base::StringValue(flimflam::kStatePortal));
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
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kDisableStubEthernet)) {
    manager->AddTechnology(flimflam::kTypeEthernet, true);
  }
  manager->AddTechnology(flimflam::kTypeWifi, true);
  manager->AddTechnology(flimflam::kTypeCellular, true);
  manager->AddTechnology(flimflam::kTypeWimax, true);

  profiles->AddProfile(kSharedProfilePath, std::string());

  // Add a wifi device.
  devices->AddDevice("stub_wifi_device1", flimflam::kTypeWifi, "/device/wifi1");

  // Add a cellular device. Used in SMS stub.
  devices->AddDevice(
      "stub_cellular_device1", flimflam::kTypeCellular, "/device/cellular1");
  devices->SetDeviceProperty("stub_cellular_device1",
                             flimflam::kCarrierProperty,
                             base::StringValue(shill::kCarrierSprint));

  // Add a wimax device.
  devices->AddDevice(
      "stub_wimax_device1", flimflam::kTypeWimax, "/device/wimax1");

  const bool add_to_visible = true;
  const bool add_to_watchlist = true;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kDisableStubEthernet)) {
    services->AddService("eth1", "eth1",
               flimflam::kTypeEthernet,
               flimflam::kStateOnline,
               add_to_visible, add_to_watchlist);
    profiles->AddService(kSharedProfilePath, "eth1");
  }

  // Wifi

  services->AddService("wifi1",
                       "wifi1",
                       flimflam::kTypeWifi,
                       flimflam::kStateOnline,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty("wifi1",
                               flimflam::kSecurityProperty,
                               base::StringValue(flimflam::kSecurityWep));
  profiles->AddService(kSharedProfilePath, "wifi1");

  services->AddService("wifi2",
                       "wifi2_PSK",
                       flimflam::kTypeWifi,
                       flimflam::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty("wifi2",
                               flimflam::kSecurityProperty,
                               base::StringValue(flimflam::kSecurityPsk));
  base::FundamentalValue strength_value(80);
  services->SetServiceProperty(
      "wifi2", flimflam::kSignalStrengthProperty, strength_value);
  profiles->AddService(kSharedProfilePath, "wifi2");

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableStubPortalledWifi)) {
    services->AddService(kStubPortalledWifiPath,
                         kStubPortalledWifiName,
                         flimflam::kTypeWifi,
                         flimflam::kStatePortal,
                         add_to_visible, add_to_watchlist);
    services->SetServiceProperty(kStubPortalledWifiPath,
                                 flimflam::kSecurityProperty,
                                 base::StringValue(flimflam::kSecurityNone));
    services->SetConnectBehavior(kStubPortalledWifiPath,
                                 base::Bind(&UpdatePortalledWifiState));
    services->SetServiceProperty(kStubPortalledWifiPath,
                                 flimflam::kConnectableProperty,
                                 base::FundamentalValue(true));
  }

  // Wimax

  services->AddService("wimax1",
                       "wimax1",
                       flimflam::kTypeWimax,
                       flimflam::kStateIdle,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "wimax1", flimflam::kConnectableProperty, base::FundamentalValue(true));

  // Cellular

  services->AddService("cellular1",
                       "cellular1",
                       flimflam::kTypeCellular,
                       flimflam::kStateIdle,
                       add_to_visible, add_to_watchlist);
  base::StringValue technology_value(flimflam::kNetworkTechnologyGsm);
  services->SetServiceProperty(
      "cellular1", flimflam::kNetworkTechnologyProperty, technology_value);
  services->SetServiceProperty(
      "cellular1",
      flimflam::kActivationStateProperty,
      base::StringValue(flimflam::kActivationStateNotActivated));
  services->SetServiceProperty("cellular1",
                               flimflam::kRoamingStateProperty,
                               base::StringValue(flimflam::kRoamingStateHome));

  // VPN

  // Set the "Provider" dictionary properties. Note: when setting these in
  // Shill, "Provider.Type", etc keys are used, but when reading the values
  // "Provider" . "Type", etc keys are used. Here we are setting the values
  // that will be read (by the UI, tests, etc).
  base::DictionaryValue provider_properties;
  provider_properties.SetString(flimflam::kTypeProperty,
                                flimflam::kProviderOpenVpn);
  provider_properties.SetString(flimflam::kHostProperty, "vpn_host");

  services->AddService("vpn1",
                       "vpn1",
                       flimflam::kTypeVPN,
                       flimflam::kStateOnline,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "vpn1", flimflam::kProviderProperty, provider_properties);
  profiles->AddService(kSharedProfilePath, "vpn1");

  services->AddService("vpn2",
                       "vpn2",
                       flimflam::kTypeVPN,
                       flimflam::kStateOffline,
                       add_to_visible, add_to_watchlist);
  services->SetServiceProperty(
      "vpn2", flimflam::kProviderProperty, provider_properties);

  manager->SortManagerServices();
}

}  // namespace shill_stub_helper
}  // namespace chromeos
