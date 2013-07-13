// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_ORIENTATION_ANDROID_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_ORIENTATION_ANDROID_H_

#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/device_data.h"

namespace content {

class DataFetcherOrientationAndroid : public DataFetcher {
 public:
  // Factory function. We'll listen for events for the lifetime of this object.
  // Returns NULL on error.
  static DataFetcher* Create();

  virtual ~DataFetcherOrientationAndroid();

  // Implementation of DataFetcher.
  virtual const DeviceData* GetDeviceData(DeviceData::Type type) OVERRIDE;

 protected:
  DataFetcherOrientationAndroid();

  DISALLOW_COPY_AND_ASSIGN(DataFetcherOrientationAndroid);
};

}  // namespace content

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_ORIENTATION_ANDROID_H_
