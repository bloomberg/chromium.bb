// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_

#include <string>
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"

class Status;

struct MobileDevice {
  MobileDevice();
  ~MobileDevice();
  scoped_ptr<DeviceMetrics> device_metrics;
  std::string user_agent;
};

Status FindMobileDevice(std::string device_name,
                        scoped_ptr<MobileDevice>* mobile_device);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
