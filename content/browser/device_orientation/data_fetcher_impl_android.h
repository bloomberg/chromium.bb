// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/device_data.h"

namespace content {

class Orientation;

// Android implementation of DeviceOrientation API.

// Android's SensorManager has a push API, whereas Chrome wants to pull data.
// To fit them together, we store incoming sensor events in a 1-element buffer.
// SensorManager calls SetOrientation() which pushes a new value (discarding the
// previous value if any). Chrome calls GetDeviceData() which reads the most
// recent value. Repeated calls to GetDeviceData() will return the same value.

class DataFetcherImplAndroid : public DataFetcher {
 public:
  // Must be called at startup, before Create().
  static void Init(JNIEnv* env);

  // Factory function. We'll listen for events for the lifetime of this object.
  // Returns NULL on error.
  static DataFetcher* Create();

  virtual ~DataFetcherImplAndroid();

  // Called from Java via JNI.
  void GotOrientation(JNIEnv*, jobject,
                      double alpha, double beta, double gamma);
  void GotAcceleration(JNIEnv*, jobject,
                       double x, double y, double z);
  void GotAccelerationIncludingGravity(JNIEnv*, jobject,
                                       double x, double y, double z);
  void GotRotationRate(JNIEnv*, jobject,
                       double alpha, double beta, double gamma);

  // Implementation of DataFetcher.
  virtual const DeviceData* GetDeviceData(DeviceData::Type type) OVERRIDE;

 private:
  DataFetcherImplAndroid();
  const Orientation* GetOrientation();

  // Wrappers for JNI methods.
  // TODO(timvolodine): move the DeviceData::Type enum to the service class
  // once it is implemented.
  bool Start(DeviceData::Type event_type, int rate_in_milliseconds);
  void Stop(DeviceData::Type event_type);

  // Value returned by GetDeviceData.
  scoped_refptr<Orientation> current_orientation_;

  // 1-element buffer, written by GotOrientation, read by GetDeviceData.
  base::Lock next_orientation_lock_;
  scoped_refptr<Orientation> next_orientation_;

  // The Java provider of orientation info.
  base::android::ScopedJavaGlobalRef<jobject> device_orientation_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherImplAndroid);
};

}  // namespace content

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_
