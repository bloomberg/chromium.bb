// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/gateway_data_provider_common.h"

#include "base/utf_string_conversions.h"

GatewayDataProviderCommon::GatewayDataProviderCommon()
    : Thread("Geolocation_gateway_provider"),
      is_first_scan_complete_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

GatewayDataProviderCommon::~GatewayDataProviderCommon() {
  // Thread must be stopped before entering destructor chain to avoid race
  // conditions; see comment in DeviceDataProvider::Unregister.
  DCHECK(!IsRunning()); // Must call StopDataProvider before destroying me.
}

bool GatewayDataProviderCommon::StartDataProvider() {
  DCHECK(CalledOnClientThread());
  DCHECK(!IsRunning());  // StartDataProvider must only be called once.
  return Start();
}

void GatewayDataProviderCommon::StopDataProvider() {
  DCHECK(CalledOnClientThread());
  Stop();
}

bool GatewayDataProviderCommon::GetData(GatewayData *data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  base::AutoLock lock(data_mutex_);
  *data = gateway_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

// Thread implementation.
void GatewayDataProviderCommon::Init() {
  DCHECK(gateway_api_ == NULL);
  gateway_api_.reset(NewGatewayApi());
  if (gateway_api_ == NULL) {
    // Error! Can't do scans, so don't try and schedule one.
    is_first_scan_complete_ = true;
    return;
  }
  DCHECK(polling_policy_ == NULL);
  polling_policy_.reset(NewPollingPolicy());
  DCHECK(polling_policy_ != NULL);
  ScheduleNextScan(0);
}

void GatewayDataProviderCommon::CleanUp() {
  // Destroy these instances in the thread on which they were created.
  gateway_api_.reset();
  polling_policy_.reset();
}

void GatewayDataProviderCommon::DoRouterScanTask() {
  bool update_available = false;
  GatewayData new_data;
  if (!gateway_api_->GetRouterData(&new_data.router_data)) {
    ScheduleNextScan(polling_policy_->NoRouterInterval());
  } else {
    {
      base::AutoLock lock(data_mutex_);
      update_available = gateway_data_.DiffersSignificantly(new_data);
      gateway_data_ = new_data;
    }
    ScheduleNextScan(polling_policy_->PollingInterval());
  }
  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    NotifyListeners();
  }
}

void GatewayDataProviderCommon::ScheduleNextScan(int interval) {
  message_loop()->PostDelayedTask(
      FROM_HERE, task_factory_.NewRunnableMethod(
          &GatewayDataProviderCommon::DoRouterScanTask), interval);
}

GatewayPollingPolicyInterface* GatewayDataProviderCommon::NewPollingPolicy() {
  return new GenericGatewayPollingPolicy;
}
