// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_WIN_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_WIN_H_

#include <SensorsApi.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/device_data.h"

namespace content {

class Orientation;

// Windows implementation of DeviceOrientation API.
// The SensorEventSink is installed to Windows sensor thread to listen for
// sensor's data. Upon each notification, DataFetcherImplWin buffers the data.
// Then Chrome sensor polling thread pulls the buffered data via GetDeviceData.
// The Inclinometer 3D sensor (SENSOR_TYPE_INCLINOMETER_3D) is used to get the
// orientation data.

class DataFetcherImplWin : public DataFetcher {
 public:
  virtual ~DataFetcherImplWin();

  // Factory function. It returns NULL on error.
  // The created object listens for events for the whole lifetime.
  static DataFetcher* Create();

  // Implement DataFetcher.
  virtual const DeviceData* GetDeviceData(DeviceData::Type type) OVERRIDE;

 private:
  class SensorEventSink;
  friend SensorEventSink;

  DataFetcherImplWin();
  bool Initialize();
  void OnOrientationData(Orientation* orientation);
  const Orientation* GetOrientation();

  base::win::ScopedComPtr<ISensor> sensor_;

  // Value returned by GetDeviceData.
  scoped_refptr<Orientation> current_orientation_;

  // The 1-element buffer follows DataFetcherImplAndroid implementation.
  // It is written by OnOrientationData and read by GetDeviceData.
  base::Lock next_orientation_lock_;
  scoped_refptr<Orientation> next_orientation_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherImplWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_WIN_H_
