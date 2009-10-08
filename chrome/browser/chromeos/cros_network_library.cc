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
    InitNetworkStatus();
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
  bool ok = true;
  if (CrosLibrary::loaded()) {
    ok = chromeos::ConnectToWifiNetwork(network.ssid.c_str(),
        password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
        GetEncryptionString(network.encryption));
  }
  if (ok) {
    // Notify all observers that connection has started.
    wifi_ssid_ = network.ssid;
    wifi_connecting_ = true;
    wifi_strength_ = network.strength;
    FOR_EACH_OBSERVER(Observer, observers_, NetworkChanged(this));
  }
}

// static
void CrosNetworkLibrary::NetworkStatusChangedHandler(void* object,
    const chromeos::ServiceInfo& service) {
  CrosNetworkLibrary* network = static_cast<CrosNetworkLibrary*>(object);
  network->ParseNetworkServiceInfo(service);
  FOR_EACH_OBSERVER(Observer, network->observers_, NetworkChanged(network));
}

void CrosNetworkLibrary::ParseNetworkServiceInfo(
    const chromeos::ServiceInfo& service) {
  DLOG(INFO) << "Parse " << service.ssid <<
                " typ=" << service.type <<
                " sta=" << service.state <<
                " pas=" << service.needs_passphrase <<
                " enc=" << service.encryption <<
                " sig=" << service.signal_strength;
  if (service.type == chromeos::TYPE_ETHERNET) {
    // Get the ethernet status.
    ethernet_connected_ = service.state == chromeos::STATE_READY;
  } else if (service.type == chromeos::TYPE_WIFI) {
    if (service.state == chromeos::STATE_READY) {
      // Record the wifi network that is connected.
      wifi_ssid_ = service.ssid;
      wifi_connecting_ = false;
      wifi_strength_ = service.signal_strength;
    } else if (service.state == chromeos::STATE_ASSOCIATION ||
               service.state == chromeos::STATE_CONFIGURATION) {
      // Record the wifi network that is connecting.
      wifi_ssid_ = service.ssid;
      wifi_connecting_ = true;
      wifi_strength_ = service.signal_strength;
    } else  {
      // A wifi network is disconnected.
      // If it is the current connected (or connecting) wifi network, then
      // we are currently disconnected from any wifi network.
      if (service.ssid == wifi_ssid_) {
        wifi_ssid_.clear();
        wifi_connecting_ = false;
        wifi_strength_ = 0;
      }
    }
  }
}

void CrosNetworkLibrary::InitNetworkStatus() {
  chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
  for (int i = 0; i < service_status->size; i++)
    ParseNetworkServiceInfo(service_status->services[i]);
  chromeos::FreeServiceStatus(service_status);
}
