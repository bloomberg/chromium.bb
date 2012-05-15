// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_
#pragma once

#include <jni.h>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/orientation.h"

namespace device_orientation {

// Android implementation of DeviceOrientation API.

// Android's SensorManager has a push API, whereas Chrome wants to pull data.
// To fit them together, we store incoming sensor events in a 1-element buffer.
// SensorManager calls SetOrientation() which pushes a new value (discarding the
// previous value if any). Chrome calls GetOrientation() which reads the most
// recent value. Repeated calls to GetOrientation() will return the same value.

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

  // Implementation of DataFetcher.
  virtual bool GetOrientation(Orientation* orientation) OVERRIDE;

 private:
  DataFetcherImplAndroid();

  // Wrappers for JNI methods.
  bool Start(int rate_in_milliseconds);
  void Stop();

  // Value returned by GetOrientation.
  scoped_ptr<Orientation> current_orientation_;

  // 1-element buffer, written by GotOrientation, read by GetOrientation.
  base::Lock next_orientation_lock_;
  scoped_ptr<Orientation> next_orientation_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherImplAndroid);
};

}  // namespace device_orientation

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_IMPL_ANDROID_H_
