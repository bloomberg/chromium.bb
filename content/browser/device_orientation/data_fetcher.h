// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_H_

#include "content/browser/device_orientation/device_data.h"

namespace content {

class DataFetcher {
 public:
  virtual ~DataFetcher() {}

  // Returns NULL if there was a fatal error getting the device data of this
  // type or if this fetcher can never provide this type of data. Otherwise,
  // returns a pointer to a DeviceData containing the most recent data.
  virtual const DeviceData* GetDeviceData(DeviceData::Type type) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_H_
