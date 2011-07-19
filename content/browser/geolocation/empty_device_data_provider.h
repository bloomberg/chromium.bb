// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H_
#pragma once

#include "content/browser/geolocation/device_data_provider.h"

// An implementation of DeviceDataProviderImplBase that does not provide any
// data. Used on platforms where a given data type is not available.

template<typename DataType>
class EmptyDeviceDataProvider : public DeviceDataProviderImplBase<DataType> {
 public:
  EmptyDeviceDataProvider() {}
  virtual ~EmptyDeviceDataProvider() {}

  // DeviceDataProviderImplBase implementation
  virtual bool StartDataProvider() { return true; }
  virtual void StopDataProvider() { }
  virtual bool GetData(DataType *data) {
    DCHECK(data);
    // This is all the data we can get - nothing.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyDeviceDataProvider);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H_
