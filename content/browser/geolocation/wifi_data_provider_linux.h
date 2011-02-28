// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
#pragma once

#include "content/browser/geolocation/wifi_data_provider_common.h"

class WifiDataProviderLinux : public WifiDataProviderCommon {
 public:
  WifiDataProviderLinux();

 private:
  virtual ~WifiDataProviderLinux();

  // WifiDataProviderCommon
  virtual WlanApiInterface* NewWlanApi();
  virtual PollingPolicyInterface* NewPollingPolicy();

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderLinux);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
