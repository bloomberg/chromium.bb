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

class WifiDataProviderChromeOs : public WifiDataProviderCommon {
 public:
  WifiDataProviderChromeOs();

  // Allows injection of |lib| for testing.
  static WlanApiInterface* NewWlanApi(chromeos::NetworkLibrary* lib);

 private:
  virtual ~WifiDataProviderChromeOs();

  // WifiDataProviderCommon
  virtual WlanApiInterface* NewWlanApi();
  virtual PollingPolicyInterface* NewPollingPolicy();

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderChromeOs);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
