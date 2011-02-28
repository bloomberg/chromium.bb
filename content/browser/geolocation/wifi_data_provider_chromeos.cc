// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "content/browser/geolocation/wifi_data_provider_chromeos.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace {
// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;  // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;  // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s
}

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

template<>
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new WifiDataProviderChromeOs();
}

WifiDataProviderChromeOs::WifiDataProviderChromeOs() {
}

WifiDataProviderChromeOs::~WifiDataProviderChromeOs() {
}

WifiDataProviderCommon::WlanApiInterface*
    WifiDataProviderChromeOs::NewWlanApi(chromeos::NetworkLibrary* lib) {
  return new chromeos::NetworkLibraryWlanApi(lib);
}

WifiDataProviderCommon::WlanApiInterface*
    WifiDataProviderChromeOs::NewWlanApi() {
  chromeos::CrosLibrary* cros_lib = chromeos::CrosLibrary::Get();
  DCHECK(cros_lib);
  if (!cros_lib->EnsureLoaded())
    return NULL;
  return NewWlanApi(cros_lib->GetNetworkLibrary());
}

PollingPolicyInterface* WifiDataProviderChromeOs::NewPollingPolicy() {
  return new GenericPollingPolicy<kDefaultPollingIntervalMilliseconds,
                                  kNoChangePollingIntervalMilliseconds,
                                  kTwoNoChangePollingIntervalMilliseconds,
                                  kNoWifiPollingIntervalMilliseconds>;
}
