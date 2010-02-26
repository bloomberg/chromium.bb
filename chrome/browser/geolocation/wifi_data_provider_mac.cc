// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We use the OSX system API function WirelessScanSplit. This function is not
// documented or included in the SDK, so we use a reverse-engineered header,
// osx_wifi_.h. This file is taken from the iStumbler project
// (http://www.istumbler.net).

#include "chrome/browser/geolocation/wifi_data_provider_mac.h"

#include <dlfcn.h>
#include <stdio.h>
#include "base/utf_string_conversions.h"
#include "chrome/browser/geolocation/osx_wifi.h"
#include "chrome/browser/geolocation/wifi_data_provider_common.h"

namespace {
// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 120000;  // 2 mins
const int kNoChangePollingInterval = 300000;  // 5 mins
const int kTwoNoChangePollingInterval = 600000;  // 10 mins

class OsxWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  OsxWlanApi();
  virtual ~OsxWlanApi();

  bool Init();

  // WlanApiInterface
  virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data);

 private:
  // Handle, context and function pointers for Apple80211 library.
  void* apple_80211_library_;
  WirelessContext* wifi_context_;
  WirelessAttachFunction WirelessAttach_function_;
  WirelessScanSplitFunction WirelessScanSplit_function_;
  WirelessDetachFunction WirelessDetach_function_;

  WifiData wifi_data_;
};

OsxWlanApi::OsxWlanApi()
    : apple_80211_library_(NULL), wifi_context_(NULL),
      WirelessAttach_function_(NULL), WirelessScanSplit_function_(NULL),
      WirelessDetach_function_(NULL) {
}

OsxWlanApi::~OsxWlanApi() {
  if (WirelessDetach_function_)
    (*WirelessDetach_function_)(wifi_context_);
  dlclose(apple_80211_library_);
}

bool OsxWlanApi::Init() {
  DLOG(INFO) << "OsxWlanApi::Init";
  apple_80211_library_ = dlopen(
      "/System/Library/PrivateFrameworks/Apple80211.framework/Apple80211",
      RTLD_LAZY);
  if (!apple_80211_library_) {
    DLOG(WARNING) << "Could not open Apple80211 library";
    return false;
  }
  WirelessAttach_function_ = reinterpret_cast<WirelessAttachFunction>(
      dlsym(apple_80211_library_, "WirelessAttach"));
  WirelessScanSplit_function_ = reinterpret_cast<WirelessScanSplitFunction>(
      dlsym(apple_80211_library_, "WirelessScanSplit"));
  WirelessDetach_function_ = reinterpret_cast<WirelessDetachFunction>(
      dlsym(apple_80211_library_, "WirelessDetach"));
  DCHECK(WirelessAttach_function_);
  DCHECK(WirelessScanSplit_function_);
  DCHECK(WirelessDetach_function_);

  if (!WirelessAttach_function_ || !WirelessScanSplit_function_ ||
      !WirelessDetach_function_) {
    DLOG(WARNING) << "Symbol error. Attach: " << !!WirelessAttach_function_
        << " Split: " << !!WirelessScanSplit_function_
        << " Detach: " << !!WirelessDetach_function_;
    return false;
  }

  WIErr err = (*WirelessAttach_function_)(&wifi_context_, 0);
  if (err != noErr) {
    DLOG(WARNING) << "Error attaching: " << err;
    return false;
  }
  return true;
}

bool OsxWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  DLOG(INFO) << "OsxWlanApi::GetAccessPointData";
  DCHECK(data);
  DCHECK(WirelessScanSplit_function_);
  CFArrayRef managed_access_points = NULL;
  CFArrayRef adhoc_access_points = NULL;
  WIErr err = (*WirelessScanSplit_function_)(wifi_context_,
                                             &managed_access_points,
                                             &adhoc_access_points,
                                             0);
  if (err != noErr) {
    DLOG(WARNING) << "Error spliting scan: " << err;
    return false;
  }

  if (managed_access_points == NULL) {
    DLOG(WARNING) << "managed_access_points == NULL";
    return false;
  }

  int num_access_points = CFArrayGetCount(managed_access_points);
  DLOG(INFO) << "Found " << num_access_points << " managed access points:-";
  for (int i = 0; i < num_access_points; ++i) {
    const WirelessNetworkInfo* access_point_info =
        reinterpret_cast<const WirelessNetworkInfo*>(
        CFDataGetBytePtr(
        reinterpret_cast<const CFDataRef>(
        CFArrayGetValueAtIndex(managed_access_points, i))));

    // Currently we get only MAC address, signal strength, channel
    // signal-to-noise and SSID
    // TODO(steveblock): Work out how to get age.
    AccessPointData access_point_data;
    access_point_data.mac_address =
        MacAddressAsString16(access_point_info->macAddress);
    // WirelessNetworkInfo::signal appears to be signal strength in dBm.
    access_point_data.radio_signal_strength = access_point_info->signal;
    access_point_data.channel = access_point_info->channel;
    // WirelessNetworkInfo::noise appears to be noise floor in dBm.
    access_point_data.signal_to_noise = access_point_info->signal -
                                        access_point_info->noise;
    if (!UTF8ToUTF16(reinterpret_cast<const char*>(access_point_info->name),
                     access_point_info->nameLen,
                     &access_point_data.ssid)) {
      access_point_data.ssid.clear();
    }

    DLOG(INFO) << "  AP " << i
        << " mac: " << access_point_data.mac_address
        << " ssid: " << access_point_data.ssid;
    data->insert(access_point_data);
  }
  return true;
}
}  // namespace

// static
template<>
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new OsxWifiDataProvider();
}

OsxWifiDataProvider::OsxWifiDataProvider() {
}

OsxWifiDataProvider::~OsxWifiDataProvider() {
}

OsxWifiDataProvider::WlanApiInterface* OsxWifiDataProvider::NewWlanApi() {
  scoped_ptr<OsxWlanApi> wlan_api(new OsxWlanApi);
  if (!wlan_api->Init()) {
   DLOG(INFO) << "OsxWifiDataProvider : failed to initialize wlan api";
   return NULL;
  }
  return wlan_api.release();
}

PollingPolicyInterface* OsxWifiDataProvider::NewPollingPolicy() {
  return new GenericPollingPolicy<kDefaultPollingInterval,
                                  kNoChangePollingInterval,
                                  kTwoNoChangePollingInterval>;
}
