// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_

#include "chrome/browser/geolocation/wifi_data_provider_common.h"

#include "base/scoped_ptr.h"

namespace chromeos { class NetworkLibrary; }

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

  scoped_ptr<chromeos::NetworkLibrary> network_library_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderChromeOs);
};

#endif  // CHROME_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_CHROMEOS_H_
