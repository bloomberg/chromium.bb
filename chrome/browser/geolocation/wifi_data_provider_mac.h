// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
#define CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_

// TODO(joth): port to chromium
#if 0

#include "gears/base/common/common.h"
#include "gears/base/common/event.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/thread.h"
#include "gears/geolocation/device_data_provider.h"
#include "gears/geolocation/osx_wifi.h"

class OsxWifiDataProvider
    : public WifiDataProviderImplBase,
      public Thread {
 public:
  OsxWifiDataProvider();
  virtual ~OsxWifiDataProvider();

  // WifiDataProviderImplBase implementation.
  virtual bool GetData(WifiData *data);

 private:
  // Thread implementation.
  virtual void Run();

  void GetAccessPointData(WifiData::AccessPointDataSet *access_points);

  // Context and function pointers for Apple80211 library.
  WirelessContextPtr wifi_context_;
  WirelessAttachFunction WirelessAttach_function_;
  WirelessScanSplitFunction WirelessScanSplit_function_;
  WirelessDetachFunction WirelessDetach_function_;

  WifiData wifi_data_;
  Mutex data_mutex_;

  // Event signalled to shut down the thread that polls for wifi data.
  Event stop_event_;

  // Whether we've successfully completed a scan for WiFi data (or the polling
  // thread has terminated early).
  bool is_first_scan_complete_;

  DISALLOW_COPY_AND_ASSIGN(OsxWifiDataProvider);
};

#endif  // 0

#endif  // CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
