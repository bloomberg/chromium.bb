// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "chrome/browser/geolocation/wifi_data_provider_chromeos.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "content/public/browser/browser_thread.h"

using content::AccessPointData;
using content::BrowserThread;
using content::GenericPollingPolicy;
using content::PollingPolicyInterface;
using content::WifiData;
using content::WifiDataProvider;
using content::WifiDataProviderCommon;
using content::WifiDataProviderImplBase;

namespace {
// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;  // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;  // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

WifiDataProviderImplBase* ChromeOSFactoryFunction() {
  return new WifiDataProviderChromeOs();
}

// This global class forces code that links in this file to use that as a data
// provider instead of the default Linux provider.
class RegisterChromeOSWifiDataProvider {
 public:
  RegisterChromeOSWifiDataProvider() {
    WifiDataProvider::SetFactory(ChromeOSFactoryFunction);
  }
};

RegisterChromeOSWifiDataProvider g_force_chrome_os_provider;

}  // namespace

namespace chromeos {
namespace {
// Wifi API binding to network_library.h, to allow reuse of the polling behavior
// defined in WifiDataProviderCommon.
class NetworkLibraryWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  // Does not transfer ownership, |lib| must remain valid for lifetime of
  // this object.
  explicit NetworkLibraryWlanApi(NetworkLibrary* lib);
  ~NetworkLibraryWlanApi();

  // WifiDataProviderCommon::WlanApiInterface
  bool GetAccessPointData(WifiData::AccessPointDataSet* data);

 private:
  NetworkLibrary* network_library_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryWlanApi);
};

NetworkLibraryWlanApi::NetworkLibraryWlanApi(NetworkLibrary* lib)
    : network_library_(lib) {
  DCHECK(network_library_ != NULL);
}

NetworkLibraryWlanApi::~NetworkLibraryWlanApi() {
}

bool NetworkLibraryWlanApi::GetAccessPointData(
    WifiData::AccessPointDataSet* result) {
  WifiAccessPointVector access_points;
  if (!network_library_->GetWifiAccessPoints(&access_points))
    return false;
  for (WifiAccessPointVector::const_iterator i = access_points.begin();
       i != access_points.end(); ++i) {
    AccessPointData ap_data;
    ap_data.mac_address = ASCIIToUTF16(i->mac_address);
    ap_data.radio_signal_strength = i->signal_strength;
    ap_data.channel = i->channel;
    ap_data.signal_to_noise = i->signal_to_noise;
    ap_data.ssid = UTF8ToUTF16(i->name);
    result->insert(ap_data);
  }
  return !result->empty() || network_library_->wifi_enabled();
}

}  // namespace
}  // namespace chromeos

WifiDataProviderChromeOs::WifiDataProviderChromeOs() : started_(false) {
}

WifiDataProviderChromeOs::~WifiDataProviderChromeOs() {
}

bool WifiDataProviderChromeOs::StartDataProvider() {
  DCHECK(CalledOnClientThread());

  DCHECK(polling_policy_ == NULL);
  polling_policy_.reset(NewPollingPolicy());
  DCHECK(polling_policy_ != NULL);

  ScheduleStart();
  return true;
}

void WifiDataProviderChromeOs::StopDataProvider() {
  DCHECK(CalledOnClientThread());

  polling_policy_.reset();
  ScheduleStop();
}

bool WifiDataProviderChromeOs::GetData(WifiData* data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  *data = wifi_data_;
  return is_first_scan_complete_;
}

WifiDataProviderCommon::WlanApiInterface*
    WifiDataProviderChromeOs::NewWlanApi(chromeos::NetworkLibrary* lib) {
  return new chromeos::NetworkLibraryWlanApi(lib);
}

WifiDataProviderCommon::WlanApiInterface*
    WifiDataProviderChromeOs::NewWlanApi() {
  chromeos::CrosLibrary* cros_lib = chromeos::CrosLibrary::Get();
  DCHECK(cros_lib);
  return NewWlanApi(cros_lib->GetNetworkLibrary());
}

PollingPolicyInterface* WifiDataProviderChromeOs::NewPollingPolicy() {
  return new GenericPollingPolicy<kDefaultPollingIntervalMilliseconds,
                                  kNoChangePollingIntervalMilliseconds,
                                  kTwoNoChangePollingIntervalMilliseconds,
                                  kNoWifiPollingIntervalMilliseconds>;
}

void WifiDataProviderChromeOs::DoStartTaskOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  wlan_api_.reset(NewWlanApi());
  if (wlan_api_ == NULL) {
    client_loop()->PostTask(
        FROM_HERE, base::Bind(&WifiDataProviderChromeOs::DidStartFailed, this));
    return;
  }
  DoWifiScanTaskOnUIThread();
}

void WifiDataProviderChromeOs::DoStopTaskOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  wlan_api_.reset();
}

void WifiDataProviderChromeOs::DidStartFailed() {
  CHECK(CalledOnClientThread());
  // Error! Can't do scans, so don't try and schedule one.
  is_first_scan_complete_ = true;
}

void WifiDataProviderChromeOs::DoWifiScanTaskOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This method could be scheduled after a DoStopTaskOnUIThread.
  if (!wlan_api_.get())
    return;

  WifiData new_data;

  if (!wlan_api_->GetAccessPointData(&new_data.access_point_data)) {
    client_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WifiDataProviderChromeOs::DidWifiScanTaskNoResults, this));
  } else {
    client_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WifiDataProviderChromeOs::DidWifiScanTask, this, new_data));
  }
}

void WifiDataProviderChromeOs::DidWifiScanTaskNoResults() {
  DCHECK(CalledOnClientThread());
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnUIThread and this method).
  if (started_)
    ScheduleNextScan(polling_policy_->NoWifiInterval());
  MaybeNotifyListeners(false);
}

void WifiDataProviderChromeOs::DidWifiScanTask(const WifiData& new_data) {
  DCHECK(CalledOnClientThread());
  bool update_available = wifi_data_.DiffersSignificantly(new_data);
  wifi_data_ = new_data;
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnUIThread and this method).
  if (started_) {
    polling_policy_->UpdatePollingInterval(update_available);
    ScheduleNextScan(polling_policy_->PollingInterval());
  }
  MaybeNotifyListeners(update_available);
}

void WifiDataProviderChromeOs::MaybeNotifyListeners(bool update_available) {
  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    NotifyListeners();
  }
}

void WifiDataProviderChromeOs::ScheduleNextScan(int interval) {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WifiDataProviderChromeOs::DoWifiScanTaskOnUIThread, this),
      base::TimeDelta::FromMilliseconds(interval));
}

void WifiDataProviderChromeOs::ScheduleStop() {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  started_ = false;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WifiDataProviderChromeOs::DoStopTaskOnUIThread, this));
}

void WifiDataProviderChromeOs::ScheduleStart() {
  DCHECK(CalledOnClientThread());
  DCHECK(!started_);
  started_ = true;
  // Perform first scan ASAP regardless of the polling policy. If this scan
  // fails we'll retry at a rate in line with the polling policy.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WifiDataProviderChromeOs::DoStartTaskOnUIThread, this));
}
