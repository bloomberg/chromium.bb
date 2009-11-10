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
      ethernet_connected_(false) {
  if (CrosLibrary::loaded()) {
    Init();
  }
  g_url_request_job_tracker.AddObserver(this);
}

NetworkLibrary::~NetworkLibrary() {
  if (CrosLibrary::loaded()) {
    chromeos::DisconnectNetworkStatus(network_status_connection_);
  }
  g_url_request_job_tracker.RemoveObserver(this);
}

// static
NetworkLibrary* NetworkLibrary::Get() {
  return Singleton<NetworkLibrary>::get();
}

// static
bool NetworkLibrary::loaded() {
  return CrosLibrary::loaded();
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
void NetworkLibrary::NetworkStatusChangedHandler(void* object,
    const chromeos::ServiceStatus& service_status) {
  NetworkLibrary* network = static_cast<NetworkLibrary*>(object);
  WifiNetworkVector networks;
  bool ethernet_connected;
  ParseNetworks(service_status, &networks, &ethernet_connected);
  network->UpdateNetworkStatus(networks, ethernet_connected);
}

// static
void NetworkLibrary::ParseNetworks(
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

void NetworkLibrary::Init() {
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

void NetworkLibrary::UpdateNetworkStatus(
    const WifiNetworkVector& networks, bool ethernet_connected) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &NetworkLibrary::UpdateNetworkStatus, networks,
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

}  // namespace chromeos
