// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_library.h"

#include <algorithm>

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"
#include "net/url_request/url_request_job.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::NetworkLibrary> {
  void RetainCallee(chromeos::NetworkLibrary* obj) {}
  void ReleaseCallee(chromeos::NetworkLibrary* obj) {}
};

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

// static
const int NetworkLibrary::kNetworkTrafficeTimerSecs = 1;

NetworkLibrary::NetworkLibrary()
    : traffic_type_(0),
      network_devices_(0),
      offline_mode_(false) {
  if (CrosLibrary::EnsureLoaded()) {
    Init();
  }
  g_url_request_job_tracker.AddObserver(this);
}

NetworkLibrary::~NetworkLibrary() {
  if (CrosLibrary::EnsureLoaded()) {
    chromeos::DisconnectNetworkStatus(network_status_connection_);
  }
  g_url_request_job_tracker.RemoveObserver(this);
}

// static
NetworkLibrary* NetworkLibrary::Get() {
  return Singleton<NetworkLibrary>::get();
}

// static
bool NetworkLibrary::EnsureLoaded() {
  return CrosLibrary::EnsureLoaded();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary, URLRequestJobTracker::JobObserver implementation:

void NetworkLibrary::OnJobAdded(URLRequestJob* job) {
  CheckNetworkTraffic(false);
}

void NetworkLibrary::OnJobRemoved(URLRequestJob* job) {
  CheckNetworkTraffic(false);
}

void NetworkLibrary::OnJobDone(URLRequestJob* job,
                               const URLRequestStatus& status) {
  CheckNetworkTraffic(false);
}

void NetworkLibrary::OnJobRedirect(URLRequestJob* job, const GURL& location,
                                   int status_code) {
  CheckNetworkTraffic(false);
}

void NetworkLibrary::OnBytesRead(URLRequestJob* job, int byte_count) {
  CheckNetworkTraffic(true);
}

void NetworkLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkLibrary::RemoveObserver(Observer* observer) {
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

void NetworkLibrary::ConnectToWifiNetwork(WifiNetwork network,
                                          const string16& password) {
  if (CrosLibrary::EnsureLoaded()) {
    // This call kicks off a request to connect to this network, the results of
    // which we'll hear about through the monitoring we've set up in Init();
    chromeos::ConnectToWifiNetwork(
        network.ssid.c_str(),
        password.empty() ? NULL : UTF16ToUTF8(password).c_str(),
        GetEncryptionString(network.encryption));
  }
}

void NetworkLibrary::ConnectToCellularNetwork(CellularNetwork network) {
  if (CrosLibrary::EnsureLoaded()) {
    // This call kicks off a request to connect to this network, the results of
    // which we'll hear about through the monitoring we've set up in Init();
    chromeos::ConnectToWifiNetwork(network.name.c_str(), NULL, NULL);
  }
}

void NetworkLibrary::EnableEthernetNetworkDevice(bool enable) {
  EnableNetworkDevice(chromeos::TYPE_ETHERNET, enable);
}

void NetworkLibrary::EnableWifiNetworkDevice(bool enable) {
  EnableNetworkDevice(chromeos::TYPE_WIFI, enable);
}

void NetworkLibrary::EnableCellularNetworkDevice(bool enable) {
  EnableNetworkDevice(chromeos::TYPE_CELLULAR, enable);
}

void NetworkLibrary::EnableOfflineMode(bool enable) {
  if (!CrosLibrary::EnsureLoaded())
    return;

  // If network device is already enabled/disabled, then don't do anything.
  if (enable && offline_mode_) {
    LOG(INFO) << "Trying to enable offline mode when it's already enabled. ";
    return;
  }
  if (!enable && !offline_mode_) {
    LOG(INFO) << "Trying to disable offline mode when it's already disabled. ";
    return;
  }

  if (chromeos::SetOfflineMode(enable)) {
    offline_mode_ = enable;
  }
}

// static
void NetworkLibrary::NetworkStatusChangedHandler(void* object,
    const chromeos::ServiceStatus& service_status) {
  NetworkLibrary* network = static_cast<NetworkLibrary*>(object);
  EthernetNetwork ethernet;
  WifiNetworkVector wifi_networks;
  CellularNetworkVector cellular_networks;
  ParseNetworks(service_status, &ethernet, &wifi_networks, &cellular_networks);
  network->UpdateNetworkStatus(ethernet, wifi_networks, cellular_networks);
}

// static
void NetworkLibrary::ParseNetworks(
    const chromeos::ServiceStatus& service_status, EthernetNetwork* ethernet,
    WifiNetworkVector* wifi_networks,
    CellularNetworkVector* cellular_networks) {
  DLOG(INFO) << "ParseNetworks:";
  for (int i = 0; i < service_status.size; i++) {
    const chromeos::ServiceInfo& service = service_status.services[i];
    DLOG(INFO) << "  (" << service.type <<
                  ") " << service.ssid <<
                  " sta=" << service.state <<
                  " pas=" << service.needs_passphrase <<
                  " enc=" << service.encryption <<
                  " sig=" << service.signal_strength;
    bool connecting = service.state == chromeos::STATE_ASSOCIATION ||
                      service.state == chromeos::STATE_CONFIGURATION ||
                      service.state == chromeos::STATE_CARRIER;
    bool connected = service.state == chromeos::STATE_READY;
    if (service.type == chromeos::TYPE_ETHERNET) {
      ethernet->connecting = connecting;
      ethernet->connected = connected;
    } else if (service.type == chromeos::TYPE_WIFI) {
      wifi_networks->push_back(WifiNetwork(service.ssid,
                                           service.needs_passphrase,
                                           service.encryption,
                                           service.signal_strength,
                                           connecting,
                                           connected));
    } else if (service.type == chromeos::TYPE_CELLULAR) {
      cellular_networks->push_back(CellularNetwork(service.ssid,
                                                   service.signal_strength,
                                                   connecting,
                                                   connected));
    }
  }
}

void NetworkLibrary::Init() {
  // First, get the currently available networks.  This data is cached
  // on the connman side, so the call should be quick.
  chromeos::ServiceStatus* service_status = chromeos::GetAvailableNetworks();
  if (service_status) {
    LOG(INFO) << "Getting initial CrOS network info.";
    EthernetNetwork ethernet;
    WifiNetworkVector wifi_networks;
    CellularNetworkVector cellular_networks;
    ParseNetworks(*service_status, &ethernet, &wifi_networks,
        &cellular_networks);
    UpdateNetworkStatus(ethernet, wifi_networks, cellular_networks);
    chromeos::FreeServiceStatus(service_status);
  }
  LOG(INFO) << "Registering for network status updates.";
  // Now, register to receive updates on network status.
  network_status_connection_ = chromeos::MonitorNetworkStatus(
      &NetworkStatusChangedHandler, this);
  // Get the enabled network devices bit flag. If we get a -1, then that means
  // offline mode is on. So all devices are disabled. This happens when offline
  // mode is persisted during a reboot and Chrome starts up with it on.
  network_devices_ = chromeos::GetEnabledNetworkDevices();
  if (network_devices_ == -1) {
    offline_mode_ = true;
    network_devices_ = 0;
  }
}

void NetworkLibrary::EnableNetworkDevice(chromeos::ConnectionType device,
                                         bool enable) {
  if (!CrosLibrary::EnsureLoaded())
    return;

  // If network device is already enabled/disabled, then don't do anything.
  if (enable && (network_devices_ & device)) {
    LOG(INFO) << "Trying to enable a network device that's already enabled: "
              << device;
    return;
  }
  if (!enable && !(network_devices_ & device)) {
    LOG(INFO) << "Trying to disable a network device that's already disabled: "
              << device;
    return;
  }

  if (chromeos::EnableNetworkDevice(device, enable)) {
    if (enable)
      network_devices_ |= device;
    else
      network_devices_ &= ~device;
  }
}

void NetworkLibrary::UpdateNetworkStatus(const EthernetNetwork& ethernet,
    const WifiNetworkVector& wifi_networks,
    const CellularNetworkVector& cellular_networks) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &NetworkLibrary::UpdateNetworkStatus, ethernet, wifi_networks,
            cellular_networks));
    return;
  }

  ethernet_ = ethernet;
  wifi_networks_ = wifi_networks;
  // Sort the list of wifi networks by ssid.
  std::sort(wifi_networks_.begin(), wifi_networks_.end());
  wifi_ = WifiNetwork();
  for (size_t i = 0; i < wifi_networks_.size(); i++) {
    if (wifi_networks_[i].connecting || wifi_networks_[i].connected) {
      wifi_ = wifi_networks_[i];
      break;  // There is only one connected or connecting wifi network.
    }
  }
  cellular_networks_ = cellular_networks;
  std::sort(cellular_networks_.begin(), cellular_networks_.end());
  cellular_ = CellularNetwork();
  for (size_t i = 0; i < cellular_networks_.size(); i++) {
    if (cellular_networks_[i].connecting || cellular_networks_[i].connected) {
      cellular_ = cellular_networks_[i];
      break;  // There is only one connected or connecting cellular network.
    }
  }
  FOR_EACH_OBSERVER(Observer, observers_, NetworkChanged(this));
}

void NetworkLibrary::CheckNetworkTraffic(bool download) {
  // If we already have a pending upload and download notification, then
  // shortcut and return.
  if (traffic_type_ == (Observer::TRAFFIC_DOWNLOAD | Observer::TRAFFIC_UPLOAD))
    return;
  // Figure out if we are uploading and/or downloading. We are downloading
  // if download == true. We are uploading if we have upload progress.
  if (download)
    traffic_type_ |= Observer::TRAFFIC_DOWNLOAD;
  if ((traffic_type_ & Observer::TRAFFIC_UPLOAD) == 0) {
    URLRequestJobTracker::JobIterator it;
    for (it = g_url_request_job_tracker.begin();
         it != g_url_request_job_tracker.end();
         ++it) {
      URLRequestJob* job = *it;
      if (job->GetUploadProgress() > 0) {
        traffic_type_ |= Observer::TRAFFIC_UPLOAD;
        break;
      }
    }
  }
  // If we have new traffic data to send out and the timer is not currently
  // running, then start a new timer.
  if (traffic_type_ && !timer_.IsRunning()) {
    timer_.Start(base::TimeDelta::FromSeconds(kNetworkTrafficeTimerSecs), this,
                 &NetworkLibrary::NetworkTrafficTimerFired);
  }
}

void NetworkLibrary:: NetworkTrafficTimerFired() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &NetworkLibrary::NotifyNetworkTraffic,
                        traffic_type_));
  // Reset traffic type so that we don't send the same data next time.
  traffic_type_ = 0;
}

void NetworkLibrary::NotifyNetworkTraffic(int traffic_type) {
  FOR_EACH_OBSERVER(Observer, observers_, NetworkTraffic(this, traffic_type));
}

bool NetworkLibrary::Connected() const {
  return ethernet_connected() || wifi_connected() || cellular_connected();
}

}  // namespace chromeos
