// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_android.h"

#include <stdint.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "jni/VideoCapture_jni.h"
#include "media/capture/video/android/video_capture_device_factory_android.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

const std::string VideoCaptureDevice::Name::GetModel() const {
  // Android cameras are not typically USB devices, and this method is currently
  // only used for USB model identifiers, so this implementation just indicates
  // an unknown device model.
  return "";
}

VideoCaptureDeviceAndroid::VideoCaptureDeviceAndroid(const Name& device_name)
    : state_(kIdle), got_first_frame_(false), device_name_(device_name) {
}

VideoCaptureDeviceAndroid::~VideoCaptureDeviceAndroid() {
  StopAndDeAllocate();
  // If there are still |photo_callbacks_|, resolve them with empty datas.
  std::for_each(photo_callbacks_.begin(), photo_callbacks_.end(),
                [](const std::unique_ptr<TakePhotoCallback>& callback) {
                  std::unique_ptr<std::vector<uint8_t>> empty_data(
                      new std::vector<uint8_t>());
                  callback->Run("", std::move(empty_data));
                });
}

bool VideoCaptureDeviceAndroid::Init() {
  int id;
  if (!base::StringToInt(device_name_.id(), &id))
    return false;

  j_capture_.Reset(VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
      id, reinterpret_cast<intptr_t>(this)));
  return true;
}

void VideoCaptureDeviceAndroid::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DVLOG(1) << "VideoCaptureDeviceAndroid::AllocateAndStart";
  {
    base::AutoLock lock(lock_);
    if (state_ != kIdle)
      return;
    client_ = std::move(client);
    got_first_frame_ = false;
  }

  JNIEnv* env = AttachCurrentThread();

  jboolean ret = Java_VideoCapture_allocate(
      env, j_capture_.obj(), params.requested_format.frame_size.width(),
      params.requested_format.frame_size.height(),
      params.requested_format.frame_rate);
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to allocate");
    return;
  }

  // Store current width and height.
  capture_format_.frame_size.SetSize(
      Java_VideoCapture_queryWidth(env, j_capture_.obj()),
      Java_VideoCapture_queryHeight(env, j_capture_.obj()));
  capture_format_.frame_rate =
      Java_VideoCapture_queryFrameRate(env, j_capture_.obj());
  capture_format_.pixel_format = GetColorspace();
  DCHECK_NE(capture_format_.pixel_format, media::PIXEL_FORMAT_UNKNOWN);
  CHECK(capture_format_.frame_size.GetArea() > 0);
  CHECK(!(capture_format_.frame_size.width() % 2));
  CHECK(!(capture_format_.frame_size.height() % 2));

  if (capture_format_.frame_rate > 0) {
    frame_interval_ = base::TimeDelta::FromMicroseconds(
        (base::Time::kMicrosecondsPerSecond + capture_format_.frame_rate - 1) /
        capture_format_.frame_rate);
  }

  DVLOG(1) << "VideoCaptureDeviceAndroid::Allocate: queried frame_size="
           << capture_format_.frame_size.ToString()
           << ", frame_rate=" << capture_format_.frame_rate;

  ret = Java_VideoCapture_startCapture(env, j_capture_.obj());
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to start capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kCapturing;
  }
}

void VideoCaptureDeviceAndroid::StopAndDeAllocate() {
  DVLOG(1) << "VideoCaptureDeviceAndroid::StopAndDeAllocate";
  {
    base::AutoLock lock(lock_);
    if (state_ != kCapturing && state_ != kError)
      return;
  }

  JNIEnv* env = AttachCurrentThread();

  jboolean ret = Java_VideoCapture_stopCapture(env, j_capture_.obj());
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to stop capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kIdle;
    client_.reset();
  }

  Java_VideoCapture_deallocate(env, j_capture_.obj());
}

bool VideoCaptureDeviceAndroid::TakePhoto(const TakePhotoCallback& callback) {
  {
    base::AutoLock lock(lock_);
    if (state_ != kCapturing)
      return false;
  }

  JNIEnv* env = AttachCurrentThread();

  // Make copy on the heap so we can pass the pointer through JNI.
  std::unique_ptr<TakePhotoCallback> cb(new TakePhotoCallback(callback));
  const intptr_t callback_id = reinterpret_cast<intptr_t>(cb.get());
  if (!Java_VideoCapture_takePhoto(env, j_capture_.obj(), callback_id))
    return false;

  photo_callbacks_.push_back(std::move(cb));
  return true;
}

void VideoCaptureDeviceAndroid::OnFrameAvailable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data,
    jint length,
    jint rotation) {
  DVLOG(3) << "VideoCaptureDeviceAndroid::OnFrameAvailable: length =" << length;

  base::AutoLock lock(lock_);
  if (state_ != kCapturing || !client_.get())
    return;

  jbyte* buffer = env->GetByteArrayElements(data, NULL);
  if (!buffer) {
    LOG(ERROR) << "VideoCaptureDeviceAndroid::OnFrameAvailable: "
                  "failed to GetByteArrayElements";
    return;
  }

  base::TimeTicks current_time = base::TimeTicks::Now();
  if (!got_first_frame_) {
    // Set aside one frame allowance for fluctuation.
    expected_next_frame_time_ = current_time - frame_interval_;
    got_first_frame_ = true;
  }

  // Deliver the frame when it doesn't arrive too early.
  if (expected_next_frame_time_ <= current_time) {
    expected_next_frame_time_ += frame_interval_;

    client_->OnIncomingCapturedData(reinterpret_cast<uint8_t*>(buffer), length,
                                    capture_format_, rotation,
                                    base::TimeTicks::Now());
  }

  env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
}

void VideoCaptureDeviceAndroid::OnError(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& message) {
  SetErrorState(FROM_HERE,
                base::android::ConvertJavaStringToUTF8(env, message));
}

void VideoCaptureDeviceAndroid::OnPhotoTaken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong callback_id,
    const base::android::JavaParamRef<jbyteArray>& data) {
  DCHECK(callback_id);

  TakePhotoCallback* const cb =
      reinterpret_cast<TakePhotoCallback*>(callback_id);
  // Search for the pointer |cb| in the list of |photo_callbacks_|.
  const auto reference_it =
      std::find_if(photo_callbacks_.begin(), photo_callbacks_.end(),
                   [cb](const std::unique_ptr<TakePhotoCallback>& callback) {
                     return callback.get() == cb;
                   });
  if (reference_it == photo_callbacks_.end()) {
    NOTREACHED() << "|callback_id| not found.";
    return;
  }

  std::unique_ptr<std::vector<uint8_t>> native_data(new std::vector<uint8_t>());
  base::android::JavaByteArrayToByteVector(env, data.obj(), native_data.get());

  cb->Run(native_data->size() ? "image/jpeg" : "", std::move(native_data));

  photo_callbacks_.erase(reference_it);
}

VideoPixelFormat VideoCaptureDeviceAndroid::GetColorspace() {
  JNIEnv* env = AttachCurrentThread();
  const int current_capture_colorspace =
      Java_VideoCapture_getColorspace(env, j_capture_.obj());
  switch (current_capture_colorspace) {
    case ANDROID_IMAGE_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case ANDROID_IMAGE_FORMAT_YUV_420_888:
      return media::PIXEL_FORMAT_I420;
    case ANDROID_IMAGE_FORMAT_NV21:
      return media::PIXEL_FORMAT_NV21;
    case ANDROID_IMAGE_FORMAT_UNKNOWN:
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

void VideoCaptureDeviceAndroid::SetErrorState(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  {
    base::AutoLock lock(lock_);
    state_ = kError;
  }
  client_->OnError(from_here, reason);
}

}  // namespace media
