// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We use the OSX system API function WirelessScanSplit. This function is not
// documented or included in the SDK, so we use a reverse-engineered header,
// osx_wifi_.h. This file is taken from the iStumbler project
// (http://www.istumbler.net).

// TODO(joth): port to chromium
#if 0

#include "gears/geolocation/wifi_data_provider_osx.h"

#include <dlfcn.h>
#include <stdio.h>
#include "gears/base/common/string_utils.h"
#include "gears/geolocation/wifi_data_provider_common.h"

// The time periods, in milliseconds, between successive polls of the wifi data.
extern const int kDefaultPollingInterval = 120000;  // 2 mins
extern const int kNoChangePollingInterval = 300000;  // 5 mins
extern const int kTwoNoChangePollingInterval = 600000;  // 10 mins

// static
template<>
WifiDataProviderImplBase *WifiDataProvider::DefaultFactoryFunction() {
  return new OsxWifiDataProvider();
}


OsxWifiDataProvider::OsxWifiDataProvider() : is_first_scan_complete_(false) {
  Start();
}

OsxWifiDataProvider::~OsxWifiDataProvider() {
  stop_event_.Signal();
  Join();
}

bool OsxWifiDataProvider::GetData(WifiData *data) {
  assert(data);
  MutexLock lock(&data_mutex_);
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

// Thread implementation
void OsxWifiDataProvider::Run() {
  void *apple_80211_library = dlopen(
      "/System/Library/PrivateFrameworks/Apple80211.framework/Apple80211",
      RTLD_LAZY);
  if (!apple_80211_library) {
    is_first_scan_complete_ = true;
    return;
  }

  WirelessAttach_function_ = reinterpret_cast<WirelessAttachFunction>(
      dlsym(apple_80211_library, "WirelessAttach"));
  WirelessScanSplit_function_ = reinterpret_cast<WirelessScanSplitFunction>(
      dlsym(apple_80211_library, "WirelessScanSplit"));
  WirelessDetach_function_ = reinterpret_cast<WirelessDetachFunction>(
      dlsym(apple_80211_library, "WirelessDetach"));
  assert(WirelessAttach_function_ &&
         WirelessScanSplit_function_ &&
         WirelessDetach_function_);

  if ((*WirelessAttach_function_)(&wifi_context_, 0) != noErr) {
    is_first_scan_complete_ = true;
    return;
  }

  // Regularly get the access point data.
  int polling_interval = kDefaultPollingInterval;
  do {
    WifiData new_data;
    GetAccessPointData(&new_data.access_point_data);
    bool update_available;
    data_mutex_.Lock();
    update_available = wifi_data_.DiffersSignificantly(new_data);
    wifi_data_ = new_data;
    data_mutex_.Unlock();
    polling_interval =
        UpdatePollingInterval(polling_interval, update_available);
    if (update_available) {
      is_first_scan_complete_ = true;
      NotifyListeners();
    }
  } while (!stop_event_.WaitWithTimeout(polling_interval));

  (*WirelessDetach_function_)(wifi_context_);

  dlclose(apple_80211_library);
}

void OsxWifiDataProvider::GetAccessPointData(
    WifiData::AccessPointDataSet *access_points) {
  assert(access_points);
  assert(WirelessScanSplit_function_);
  CFArrayRef managed_access_points = NULL;
  CFArrayRef adhoc_access_points = NULL;
  if ((*WirelessScanSplit_function_)(wifi_context_,
                                     &managed_access_points,
                                     &adhoc_access_points,
                                     0) != noErr) {
    return;
  }

  if (managed_access_points == NULL) {
    return;
  }

  int num_access_points = CFArrayGetCount(managed_access_points);
  for (int i = 0; i < num_access_points; ++i) {
    const WirelessNetworkInfo *access_point_info =
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
    string16 ssid;
    if (UTF8ToString16(reinterpret_cast<const char*>(access_point_info->name),
                       access_point_info->nameLen,
                       &ssid)) {
      access_point_data.ssid = ssid;
    }

    access_points->insert(access_point_data);
  }
}

#endif  // 0
