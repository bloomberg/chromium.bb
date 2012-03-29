// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_ACCELEROMETER_MAC_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_ACCELEROMETER_MAC_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/device_orientation/data_fetcher.h"

class SuddenMotionSensor;

namespace device_orientation {

class AccelerometerMac : public DataFetcher {
 public:
  static DataFetcher* Create();

  // Implement DataFetcher.
  virtual bool GetOrientation(Orientation* orientation) OVERRIDE;

  virtual ~AccelerometerMac();

 private:
  AccelerometerMac();
  bool Init();

  scoped_ptr<SuddenMotionSensor> sudden_motion_sensor_;
};

}  // namespace device_orientation

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_ACCELEROMETER_MAC_H_
