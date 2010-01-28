// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
#define CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_

#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/geolocation/device_data_provider.h"

class PollingPolicyInterface;

class Win32WifiDataProvider
    : public WifiDataProviderImplBase,
      private base::Thread {
 public:
  // Interface to abstract the low level data OS library call, and to allow
  // mocking (hence public).
  class WlanApiInterface {
   public:
    virtual ~WlanApiInterface() {}
    // Gets wifi data for all visible access points.
    virtual bool GetAccessPointData(WifiData::AccessPointDataSet *data) = 0;
  };

  Win32WifiDataProvider();
  virtual ~Win32WifiDataProvider();

  // Takes ownership of wlan_api. Must be called before Start().
  void inject_mock_wlan_api(WlanApiInterface* wlan_api);

  // Takes ownership of polic. Must be called before Start().
  void inject_mock_polling_policy(PollingPolicyInterface* policy);

  // WifiDataProviderImplBase implementation
  virtual bool StartDataProvider();
  virtual bool GetData(WifiData *data);

 private:
  // Thread implementation
  virtual void Init();
  // Called just after the message loop ends
  virtual void CleanUp();

  // Task which run in the child thread.
  void DoWifiScanTask();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan();

  WifiData wifi_data_;
  Lock data_mutex_;

  // Whether we've successfully completed a scan for WiFi data (or the polling
  // thread has terminated early).
  bool is_first_scan_complete_;

  // Underlying OS wifi API.
  scoped_ptr<WlanApiInterface> wlan_api_;

  // Controls the polling update interval.
  scoped_ptr<PollingPolicyInterface> polling_policy_;

  // Holder for the tasks which run on the thread; takes care of cleanup.
  ScopedRunnableMethodFactory<Win32WifiDataProvider> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(Win32WifiDataProvider);
};

#endif  // CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
