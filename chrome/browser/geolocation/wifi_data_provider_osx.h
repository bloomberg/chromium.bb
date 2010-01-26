// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GEARS_GEOLOCATION_WIFI_DATA_PROVIDER_OSX_H__
#define GEARS_GEOLOCATION_WIFI_DATA_PROVIDER_OSX_H__

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

  DISALLOW_EVIL_CONSTRUCTORS(OsxWifiDataProvider);
};

#endif  // GEARS_GEOLOCATION_WIFI_DATA_PROVIDER_OSX_H__
