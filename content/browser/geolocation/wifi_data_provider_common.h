// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
#pragma once

#include <assert.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/device_data_provider.h"

// Converts a MAC address stored as an array of uint8 to a string.
string16 MacAddressAsString16(const uint8 mac_as_int[6]);

// Allows sharing and mocking of the update polling policy function.
class PollingPolicyInterface {
 public:
  virtual ~PollingPolicyInterface() {}
  // Calculates the new polling interval for wiFi scans, given the previous
  // interval and whether the last scan produced new results.
  virtual void UpdatePollingInterval(bool scan_results_differ) = 0;
  virtual int PollingInterval() = 0;
  virtual int NoWifiInterval() = 0;
};

// Generic polling policy, constants are compile-time parameterized to allow
// tuning on a per-platform basis.
template<int DEFAULT_INTERVAL,
         int NO_CHANGE_INTERVAL,
         int TWO_NO_CHANGE_INTERVAL,
         int NO_WIFI_INTERVAL>
class GenericPollingPolicy : public PollingPolicyInterface {
 public:
  GenericPollingPolicy() : polling_interval_(DEFAULT_INTERVAL) {}
  // PollingPolicyInterface
  virtual void UpdatePollingInterval(bool scan_results_differ) {
    if (scan_results_differ) {
      polling_interval_ = DEFAULT_INTERVAL;
    } else if (polling_interval_ == DEFAULT_INTERVAL) {
      polling_interval_ = NO_CHANGE_INTERVAL;
    } else {
      DCHECK(polling_interval_ == NO_CHANGE_INTERVAL ||
             polling_interval_ == TWO_NO_CHANGE_INTERVAL);
      polling_interval_ = TWO_NO_CHANGE_INTERVAL;
    }
  }
  virtual int PollingInterval() { return polling_interval_; }
  virtual int NoWifiInterval() { return NO_WIFI_INTERVAL; }

 private:
  int polling_interval_;
};

// Base class to promote code sharing between platform specific wifi data
// providers. It's optional for specific platforms to derive this, but if they
// do threading and polling is taken care of by this base class, and all the
// platform need do is provide the underlying WLAN access API and policy policy,
// both of which will be create & accessed in the worker thread (only).
// Also designed this way to promotes ease of testing the cross-platform
// behavior w.r.t. polling & threading.
class WifiDataProviderCommon
    : public WifiDataProviderImplBase,
      private base::Thread {
 public:
  // Interface to abstract the low level data OS library call, and to allow
  // mocking (hence public).
  class WlanApiInterface {
   public:
    virtual ~WlanApiInterface() {}
    // Gets wifi data for all visible access points.
    virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data) = 0;
  };

  WifiDataProviderCommon();

  // WifiDataProviderImplBase implementation
  virtual bool StartDataProvider();
  virtual void StopDataProvider();
  virtual bool GetData(WifiData *data);

 protected:
  virtual ~WifiDataProviderCommon();

  // Returns ownership. Will be called from the worker thread.
  virtual WlanApiInterface* NewWlanApi() = 0;

  // Returns ownership. Will be called from the worker thread.
  virtual PollingPolicyInterface* NewPollingPolicy() = 0;

 private:
  // Thread implementation
  virtual void Init();
  virtual void CleanUp();

  // Task which run in the child thread.
  void DoWifiScanTask();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  WifiData wifi_data_;
  base::Lock data_mutex_;

  // Whether we've successfully completed a scan for WiFi data (or the polling
  // thread has terminated early).
  bool is_first_scan_complete_;

  // Underlying OS wifi API.
  scoped_ptr<WlanApiInterface> wlan_api_;

  // Controls the polling update interval.
  scoped_ptr<PollingPolicyInterface> polling_policy_;

  // Holder for the tasks which run on the thread; takes care of cleanup.
  ScopedRunnableMethodFactory<WifiDataProviderCommon> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderCommon);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
