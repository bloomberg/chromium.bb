// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/geolocation/wifi_data_provider_common.h"
#include "content/common/content_export.h"

namespace dbus {
class Bus;
};

class CONTENT_EXPORT WifiDataProviderLinux : public WifiDataProviderCommon {
 public:
  WifiDataProviderLinux();

 private:
  friend class GeolocationWifiDataProviderLinuxTest;

  virtual ~WifiDataProviderLinux();

  // WifiDataProviderCommon
  virtual WlanApiInterface* NewWlanApi() OVERRIDE;
  virtual PollingPolicyInterface* NewPollingPolicy() OVERRIDE;

  // For testing.
  WlanApiInterface* NewWlanApiForTesting(dbus::Bus* bus);

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderLinux);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_LINUX_H_
