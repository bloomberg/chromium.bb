// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_orientation_android.h"

#include "base/logging.h"
#include "content/browser/device_orientation/data_fetcher_impl_android.h"

namespace content {

DataFetcherOrientationAndroid::DataFetcherOrientationAndroid() {
}

DataFetcherOrientationAndroid::~DataFetcherOrientationAndroid() {
  DataFetcherImplAndroid::GetInstance()->Stop(DeviceData::kTypeOrientation);
}

DataFetcher* DataFetcherOrientationAndroid::Create() {
  scoped_ptr<DataFetcherOrientationAndroid> fetcher(
      new DataFetcherOrientationAndroid);
  if (DataFetcherImplAndroid::GetInstance()->Start(
      DeviceData::kTypeOrientation))
    return fetcher.release();

  DVLOG(2) << "DataFetcherImplAndroid::Start failed!";
  return NULL;
}

const DeviceData* DataFetcherOrientationAndroid::GetDeviceData(
    DeviceData::Type type) {
  if (type != DeviceData::kTypeOrientation)
    return NULL;
  return DataFetcherImplAndroid::GetInstance()->GetDeviceData(type);
}

}  // namespace content
