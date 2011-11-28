// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/geolocation/wifi_data_provider_common.h"

namespace chromeos {
class NetworkLibrary;
}

class WifiDataProviderChromeOs : public WifiDataProviderImplBase {
 public:
  WifiDataProviderChromeOs();

  // WifiDataProviderImplBase
  virtual bool StartDataProvider() OVERRIDE;
  virtual void StopDataProvider() OVERRIDE;
  virtual bool GetData(WifiData* data) OVERRIDE;

  // Allows injection of |lib| for testing.
  static WifiDataProviderCommon::WlanApiInterface* NewWlanApi(
      chromeos::NetworkLibrary* lib);

 private:
  virtual ~WifiDataProviderChromeOs();

  // UI thread
  void DoWifiScanTaskOnUIThread();  // The polling task
  void DoStartTaskOnUIThread();
  void DoStopTaskOnUIThread();

  // Client thread
  void DidWifiScanTaskNoResults();
  void DidWifiScanTask(const WifiData& new_data);
  void MaybeNotifyListeners(bool update_available);
  void DidStartFailed();

  // WifiDataProviderCommon
  virtual WifiDataProviderCommon::WlanApiInterface* NewWlanApi();
  virtual PollingPolicyInterface* NewPollingPolicy();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  // Will schedule starting of the scanning process.
  void ScheduleStart();

  // Will schedule stopping of the scanning process.
  void ScheduleStop();

  // Underlying OS wifi API. (UI thread)
  scoped_ptr<WifiDataProviderCommon::WlanApiInterface> wlan_api_;

  // Controls the polling update interval. (client thread)
  scoped_ptr<PollingPolicyInterface> polling_policy_;

  // The latest wifi data. (client thread)
  WifiData wifi_data_;

  // Whether we have strated the data provider. (client thread)
  bool started_;

  // Whether we've successfully completed a scan for WiFi data. (client thread)
  bool is_first_scan_complete_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderChromeOs);
};

#endif  // CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
