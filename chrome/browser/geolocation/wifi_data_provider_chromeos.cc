// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "chrome/browser/geolocation/wifi_data_provider_chromeos.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace {
// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;  // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;  // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
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
    WifiData::AccessPointDataSet* data) {
  const WifiNetworkVector& networks = network_library_->wifi_networks();
  for (WifiNetworkVector::const_iterator i = networks.begin();
       i != networks.end(); ++i) {
    for (WifiNetwork::AccessPointVector::const_iterator j =
         i->access_points().begin(); j != i->access_points().end(); ++j) {
      AccessPointData ap_data;
      ap_data.mac_address = ASCIIToUTF16(j->mac_address);
      ap_data.radio_signal_strength = j->signal_strength;
      ap_data.channel = j->channel;
      ap_data.signal_to_noise = j->signal_to_noise;
      ap_data.ssid = UTF8ToUTF16(i->name());
      data->insert(ap_data);
    }
  }
  return !data->empty() || network_library_->wifi_available();
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
  if (network_library_ == NULL) {
    network_library_.reset(new chromeos::NetworkLibraryImpl());
    // TODO(joth): Check net_lib loaded ok, if not return NULL.
  }
  return NewWlanApi(network_library_.get());
}

PollingPolicyInterface* WifiDataProviderChromeOs::NewPollingPolicy() {
  return new GenericPollingPolicy<kDefaultPollingIntervalMilliseconds,
                                  kNoChangePollingIntervalMilliseconds,
                                  kTwoNoChangePollingIntervalMilliseconds>;
}
