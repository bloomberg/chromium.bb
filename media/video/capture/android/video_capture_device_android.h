// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
#define MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/base/media_export.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

// VideoCaptureDevice on Android. The VideoCaptureDevice API's are called
// by VideoCaptureManager on its own thread, while OnFrameAvailable is called
// on JAVA thread (i.e., UI thread). Both will access |state_| and |observer_|,
// but only VideoCaptureManager would change their value.
class MEDIA_EXPORT VideoCaptureDeviceAndroid : public VideoCaptureDevice {
 public:
  virtual ~VideoCaptureDeviceAndroid();

  static VideoCaptureDevice* Create(const Name& device_name);
  static bool RegisterVideoCaptureDevice(JNIEnv* env);

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;
  virtual const Name& device_name() OVERRIDE;

  // Implement org.chromium.media.VideoCapture.nativeOnFrameAvailable.
  void OnFrameAvailable(
      JNIEnv* env,
      jobject obj,
      jbyteArray data,
      jint length,
      jint rotation,
      jboolean flip_vert,
      jboolean flip_horiz);

 private:
  enum InternalState {
    kIdle,  // The device is opened but not in use.
    kAllocated,  // All resouces have been allocated and camera can be started.
    kCapturing,  // Video is being captured.
    kError  // Hit error. User needs to recover by destroying the object.
  };

  explicit VideoCaptureDeviceAndroid(const Name& device_name);
  bool Init();
  void SetErrorState(const std::string& reason);

  // Prevent racing on accessing |state_| and |observer_| since both could be
  // accessed from different threads.
  base::Lock lock_;
  InternalState state_;
  VideoCaptureDevice::EventHandler* observer_;

  Name device_name_;
  VideoCaptureCapability current_settings_;
  scoped_ptr<uint8[]> rotation_buffer_;
  int rotation_;

  // Java VideoCaptureAndroid instance.
  base::android::ScopedJavaGlobalRef<jobject> j_capture_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceAndroid);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
