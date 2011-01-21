// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/wifi_data_provider_common.h"

#include "base/utf_string_conversions.h"

string16 MacAddressAsString16(const uint8 mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static const wchar_t* const kMacFormatString =
      L"%02x-%02x-%02x-%02x-%02x-%02x";
  return WideToUTF16(StringPrintf(kMacFormatString,
                                  mac_as_int[0], mac_as_int[1], mac_as_int[2],
                                  mac_as_int[3], mac_as_int[4], mac_as_int[5]));
}

WifiDataProviderCommon::WifiDataProviderCommon()
    : Thread("Geolocation_wifi_provider"),
      is_first_scan_complete_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

WifiDataProviderCommon::~WifiDataProviderCommon() {
  // Thread must be stopped before entering destructor chain to avoid race
  // conditions; see comment in DeviceDataProvider::Unregister.
  DCHECK(!IsRunning());  // Must call StopDataProvider before destroying me.
}

bool WifiDataProviderCommon::StartDataProvider() {
  DCHECK(CalledOnClientThread());
  DCHECK(!IsRunning());  // StartDataProvider must only be called once.
  return Start();
}

void WifiDataProviderCommon::StopDataProvider() {
  DCHECK(CalledOnClientThread());
  Stop();
}

bool WifiDataProviderCommon::GetData(WifiData *data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  base::AutoLock lock(data_mutex_);
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

// Thread implementation
void WifiDataProviderCommon::Init() {
  DCHECK(wlan_api_ == NULL);
  wlan_api_.reset(NewWlanApi());
  if (wlan_api_ == NULL) {
    // Error! Can't do scans, so don't try and schedule one.
    is_first_scan_complete_ = true;
    return;
  }

  DCHECK(polling_policy_ == NULL);
  polling_policy_.reset(NewPollingPolicy());
  DCHECK(polling_policy_ != NULL);

  // Perform first scan ASAP regardless of the polling policy. If this scan
  // fails we'll retry at a rate in line with the polling policy.
  ScheduleNextScan(0);
}

void WifiDataProviderCommon::CleanUp() {
  // Destroy these instances in the thread on which they were created.
  wlan_api_.reset();
  polling_policy_.reset();
}

void WifiDataProviderCommon::DoWifiScanTask() {
  bool update_available = false;
  WifiData new_data;
  if (!wlan_api_->GetAccessPointData(&new_data.access_point_data)) {
    ScheduleNextScan(polling_policy_->NoWifiInterval());
  } else {
    {
      base::AutoLock lock(data_mutex_);
      update_available = wifi_data_.DiffersSignificantly(new_data);
      wifi_data_ = new_data;
    }
    polling_policy_->UpdatePollingInterval(update_available);
    ScheduleNextScan(polling_policy_->PollingInterval());
  }
  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    NotifyListeners();
  }
}

void WifiDataProviderCommon::ScheduleNextScan(int interval) {
  message_loop()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(&WifiDataProviderCommon::DoWifiScanTask),
      interval);
}
