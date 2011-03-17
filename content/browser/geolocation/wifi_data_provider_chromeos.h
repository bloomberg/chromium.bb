// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#pragma once

#include "content/browser/geolocation/wifi_data_provider_common.h"

namespace chromeos {
class NetworkLibrary;
}

class WifiDataProviderChromeOs : public WifiDataProviderImplBase {
 public:
  WifiDataProviderChromeOs();

  // WifiDataProviderImplBase
  virtual bool StartDataProvider();
  virtual void StopDataProvider();
  virtual bool GetData(WifiData* data);

  // Allows injection of |lib| for testing.
  static WifiDataProviderCommon::WlanApiInterface* NewWlanApi(
      chromeos::NetworkLibrary* lib);

 private:
  virtual ~WifiDataProviderChromeOs();

  // The polling task
  void DoWifiScanTask();
  void DidWifiScanTaskNoResults();
  void DidWifiScanTask(const WifiData& new_data);
  void MaybeNotifyListeners(bool update_available);

  // WifiDataProviderCommon
  virtual WifiDataProviderCommon::WlanApiInterface* NewWlanApi();
  virtual PollingPolicyInterface* NewPollingPolicy();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  // Underlying OS wifi API.
  scoped_ptr<WifiDataProviderCommon::WlanApiInterface> wlan_api_;

  // Controls the polling update interval.
  scoped_ptr<PollingPolicyInterface> polling_policy_;

  WifiData wifi_data_;

  // Whether we have strated the data provider.
  bool started_;

  // Whether we've successfully completed a scan for WiFi data.
  bool is_first_scan_complete_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderChromeOs);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
