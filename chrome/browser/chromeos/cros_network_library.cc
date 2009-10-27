// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_network_library.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<CrosNetworkLibrary> {
  void RetainCallee(CrosNetworkLibrary* obj) {}
  void ReleaseCallee(CrosNetworkLibrary* obj) {}
};

CrosNetworkLibrary::CrosNetworkLibrary() {
  if (CrosLibrary::loaded()) {
    Init();
  }
}

CrosNetworkLibrary::~CrosNetworkLibrary() {
  if (CrosLibrary::loaded()) {
    chromeos::DisconnectNetworkStatus(network_status_connection_);
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
  if (CrosLibrary::loaded()) {
    // This call kicks off a request to connect to this network, the results of
    // which we'll hear about through the monitoring we've set up in Init();
    chromeos::ConnectToWifiNetwork(
        network.ssid.c_str(),
        password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
        GetEncryptionString(network.encryption));
  }
}

// static
void CrosNetworkLibrary::NetworkStatusChangedHandler(void* object,
    const chromeos::ServiceStatus& service_status) {
  CrosNetworkLibrary* network = static_cast<CrosNetworkLibrary*>(object);
  WifiNetworkVector networks;
  bool ethernet_connected;
  ParseNetworks(service_status, &networks, &ethernet_connected);
  network->UpdateNetworkStatus(networks, ethernet_connected);
}

// static
void CrosNetworkLibrary::ParseNetworks(
    const chromeos::ServiceStatus& service_status, WifiNetworkVector* networks,
    bool* ethernet_connected) {
  *ethernet_connected = false;
  for (int i = 0; i < service_status.size; i++) {
    const chromeos::ServiceInfo& service = service_status.services[i];
    DLOG(INFO) << "Parse " << service.ssid <<
                  " typ=" << service.type <<
                  " sta=" << service.state <<
                  " pas=" << service.needs_passphrase <<
                  " enc=" << service.encryption <<
                  " sig=" << service.signal_strength;
    if (service.type == chromeos::TYPE_ETHERNET) {
      // Get the ethernet status.
      *ethernet_connected = service.state == chromeos::STATE_READY;
    } else if (service.type == chromeos::TYPE_WIFI) {
      bool connecting = service.state == chromeos::STATE_ASSOCIATION ||
                        service.state == chromeos::STATE_CONFIGURATION;
      bool connected = service.state == chromeos::STATE_READY;
      networks->push_back(WifiNetwork(service.ssid,
                                      service.needs_passphrase,
                                      service.encryption,
                                      service.signal_strength,
                                      connecting,
                                      connected));
    }
  }
}

void CrosNetworkLibrary::Init() {
  // First, get the currently available networks.  This data is cached
  // on the connman side, so the call should be quick.
  chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
  if (service_status) {
    LOG(INFO) << "Getting initial CrOS network info.";
    WifiNetworkVector networks;
    bool ethernet_connected;
    ParseNetworks(*service_status, &networks, &ethernet_connected);
    UpdateNetworkStatus(networks, ethernet_connected);
    chromeos::FreeServiceStatus(service_status);
  }
  LOG(INFO) << "Registering for network status updates.";
  // Now, register to receive updates on network status.
  network_status_connection_ = chromeos::MonitorNetworkStatus(
      &NetworkStatusChangedHandler, this);
}

void CrosNetworkLibrary::UpdateNetworkStatus(
    const WifiNetworkVector& networks, bool ethernet_connected) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    MessageLoop* loop = ChromeThread::GetMessageLoop(ChromeThread::UI);
    if (loop)
      loop->PostTask(FROM_HERE, NewRunnableMethod(this,
          &CrosNetworkLibrary::UpdateNetworkStatus, networks,
          ethernet_connected));
    return;
  }

  ethernet_connected_ = ethernet_connected;
  wifi_networks_ = networks;
  // Sort the list of wifi networks by ssid.
  std::sort(wifi_networks_.begin(), wifi_networks_.end());
  wifi_ = WifiNetwork();
  for (size_t i = 0; i < wifi_networks_.size(); i++) {
    if (wifi_networks_[i].connecting || wifi_networks_[i].connected) {
      wifi_ = wifi_networks_[i];
      break;  // There is only one connected or connecting wifi network.
    }
  }
  FOR_EACH_OBSERVER(Observer, observers_, NetworkChanged(this));
}
