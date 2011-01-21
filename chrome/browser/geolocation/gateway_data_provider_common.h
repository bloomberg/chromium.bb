// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_COMMON_H_
#define CHROME_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_COMMON_H_
#pragma once

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "chrome/browser/geolocation/device_data_provider.h"

namespace {
const int kDefaultInterval = 20 * 60 * 1000; // 20 mins
const int kNoRouterInterval = 30 * 1000; // 30s
}

// Allows sharing and mocking of the update polling policy function.
class GatewayPollingPolicyInterface {
 public:
  virtual ~GatewayPollingPolicyInterface() {}

  // Returns the polling interval to be used when we are connected to a router
  // via Ethernet.
  virtual int PollingInterval() = 0;
  // Returns the polling interval to be used when there are no adapters
  // connected to a router via Ethernet.
  virtual int NoRouterInterval() = 0;
};

// Base class to promote sharing between platform specific gateway data
// providers.
// TODO(allanwoj): This class and WifiDataProviderCommon are extremely similar
// so we might as well derive all data providers from a single base class
// generalised for different network data types.
class GatewayDataProviderCommon
    : public GatewayDataProviderImplBase,
      private base::Thread {
 public:
  // Interface to abstract the low level data OS library call, and to allow
  // mocking (hence public).
  class GatewayApiInterface {
   public:
    virtual ~GatewayApiInterface() {}
    // Gets wifi data for all visible access points.
    virtual bool GetRouterData(GatewayData::RouterDataSet* data) = 0;
  };

  GatewayDataProviderCommon();

  // GatewayDataProviderImplBase implementation.
  virtual bool StartDataProvider();
  virtual void StopDataProvider();
  virtual bool GetData(GatewayData *data);

 protected:
  virtual ~GatewayDataProviderCommon();

  // Returns ownership. Will be called from the worker thread.
  virtual GatewayApiInterface* NewGatewayApi() = 0;
  // Returns ownership. Will be called from the worker thread.
  virtual GatewayPollingPolicyInterface* NewPollingPolicy();

 private:
  class GenericGatewayPollingPolicy : public GatewayPollingPolicyInterface {
   public:
    GenericGatewayPollingPolicy() {}

    // GatewayPollingPolicyInterface
    virtual int PollingInterval() { return kDefaultInterval; }
    virtual int NoRouterInterval() { return kNoRouterInterval; }
  };

  // Thread implementation.
  virtual void Init();
  virtual void CleanUp();

  // Task which run in the child thread.
  void DoRouterScanTask();
  // Will schedule a scan; i.e. enqueue DoRouterScanTask deferred task.
  void ScheduleNextScan(int interval);

  base::Lock data_mutex_;
  // Whether we've successfully completed a scan for gateway data (or the
  // polling thread has terminated early).
  bool is_first_scan_complete_;
  // Underlying OS gateway API.
  scoped_ptr<GatewayApiInterface> gateway_api_;
  GatewayData gateway_data_;
  // Controls the polling update interval.
  scoped_ptr<GatewayPollingPolicyInterface> polling_policy_;
  // Holder for the tasks which run on the thread; takes care of cleanup.
  ScopedRunnableMethodFactory<GatewayDataProviderCommon> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(GatewayDataProviderCommon);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GATEWAY_DATA_PROVIDER_COMMON_H_
