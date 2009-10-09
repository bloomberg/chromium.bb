// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_network_library.h"

#include <algorithm>

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros_library.h"

CrosNetworkLibrary::CrosNetworkLibrary()
  : ethernet_connected_(false),
    wifi_connecting_(false),
    wifi_strength_(0) {
  if (CrosLibrary::loaded()) {
    chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
    UpdateNetworkServiceStatus(*service_status);
    chromeos::FreeServiceStatus(service_status);
    chromeos::MonitorNetworkStatus(&NetworkStatusChangedHandler, this);
  }
}

// static
CrosNetworkLibrary* CrosNetworkLibrary::Get() {
  return Singleton<CrosNetworkLibrary>::get();
}

// static
bool CrosNetworkLibrary::loaded() {
  return CrosLibrary::loaded();
}

void CrosNetworkLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrosNetworkLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

WifiNetworkVector CrosNetworkLibrary::GetWifiNetworks() {
  WifiNetworkVector networks;
  if (CrosLibrary::loaded()) {
    chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
    for (int i = 0; i < service_status->size; i++) {
      const chromeos::ServiceInfo& service = service_status->services[i];
      if (service.type == chromeos::TYPE_WIFI) {
        // Found a wifi network, add it to the list.
        networks.push_back(WifiNetwork(service.ssid, service.needs_passphrase,
                                       service.encryption,
                                       service.signal_strength));
      }
    }
    chromeos::FreeServiceStatus(service_status);
  }

  // Sort the list of wifi networks by ssid.
  std::sort(networks.begin(), networks.end());

  return networks;
}

static const char* GetEncryptionString(chromeos::EncryptionType encryption) {
  switch (encryption) {
    case chromeos::NONE:
      return "none";
    case chromeos::RSN:
      return "rsn";
    case chromeos::WEP:
      return "wep";
    case chromeos::WPA:
      return "wpa";
  }
  return "none";
}

void CrosNetworkLibrary::ConnectToWifiNetwork(WifiNetwork network,
                                              const string16& password) {
  if (CrosLibrary::loaded())
    chromeos::ConnectToWifiNetwork(network.ssid.c_str(),
        password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
        GetEncryptionString(network.encryption));
}

// static
void CrosNetworkLibrary::NetworkStatusChangedHandler(void* object,
    const chromeos::ServiceStatus& service_status) {
  CrosNetworkLibrary* network = static_cast<CrosNetworkLibrary*>(object);
  network->UpdateNetworkServiceStatus(service_status);
  FOR_EACH_OBSERVER(Observer, network->observers_, NetworkChanged(network));
}

void CrosNetworkLibrary::UpdateNetworkServiceStatus(
    const chromeos::ServiceStatus& service_status) {
  // Loop through the services and figure out what the current state is.
  bool found_ethernet = false;
  bool found_wifi = false;
  for (int i = 0; i < service_status.size; i++) {
    const chromeos::ServiceInfo& service = service_status.services[i];
    DLOG(INFO) << "Parse " << service.ssid <<
                  " typ=" << service.type <<
                  " sta=" << service.state <<
                  " pas=" << service.needs_passphrase <<
                  " enc=" << service.encryption <<
                  " sig=" << service.signal_strength;
    if (service.type == chromeos::TYPE_ETHERNET) {
      found_ethernet = true;
      // Get the ethernet status.
      ethernet_connected_ = service.state == chromeos::STATE_READY;
    } else if (service.type == chromeos::TYPE_WIFI) {
      if (service.state == chromeos::STATE_READY ||
          service.state == chromeos::STATE_ASSOCIATION ||
          service.state == chromeos::STATE_CONFIGURATION) {
        found_wifi = true;
        // Record the wifi network that is connected.
        wifi_ssid_ = service.ssid;
        wifi_connecting_ = service.state != chromeos::STATE_READY;
        wifi_strength_ = service.signal_strength;
      }
    }
  }
  if (!found_ethernet)
    ethernet_connected_ = false;
  if (!found_wifi) {
    wifi_ssid_.clear();
    wifi_connecting_ = false;
    wifi_strength_ = 0;
  }
}
