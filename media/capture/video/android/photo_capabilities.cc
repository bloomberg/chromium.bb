// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/photo_capabilities.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/PhotoCapabilities_jni.h"

using base::android::AttachCurrentThread;

namespace media {

PhotoCapabilities::PhotoCapabilities(
    base::android::ScopedJavaLocalRef<jobject> object)
    : object_(object) {}

PhotoCapabilities::~PhotoCapabilities() {}

int PhotoCapabilities::getMinIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinIso(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMaxIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxIso(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getCurrentIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentIso(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMinHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinHeight(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMaxHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxHeight(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getCurrentHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentHeight(AttachCurrentThread(),
                                                 object_);
}

int PhotoCapabilities::getMinWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinWidth(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMaxWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxWidth(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getCurrentWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentWidth(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMinZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinZoom(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMaxZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxZoom(AttachCurrentThread(), object_);
}

int PhotoCapabilities::getCurrentZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentZoom(AttachCurrentThread(), object_);
}

PhotoCapabilities::AndroidMeteringMode PhotoCapabilities::getFocusMode() const {
  DCHECK(!object_.is_null());
  return static_cast<AndroidMeteringMode>(
      Java_PhotoCapabilities_getFocusMode(AttachCurrentThread(), object_));
}

PhotoCapabilities::AndroidMeteringMode PhotoCapabilities::getExposureMode()
    const {
  DCHECK(!object_.is_null());
  return static_cast<AndroidMeteringMode>(
      Java_PhotoCapabilities_getExposureMode(AttachCurrentThread(), object_));
}

int PhotoCapabilities::getMinExposureCompensation() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinExposureCompensation(
      AttachCurrentThread(), object_);
}

int PhotoCapabilities::getMaxExposureCompensation() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxExposureCompensation(
      AttachCurrentThread(), object_);
}

int PhotoCapabilities::getCurrentExposureCompensation() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentExposureCompensation(
      AttachCurrentThread(), object_);
}

PhotoCapabilities::AndroidMeteringMode PhotoCapabilities::getWhiteBalanceMode()
    const {
  DCHECK(!object_.is_null());
  return static_cast<AndroidMeteringMode>(
      Java_PhotoCapabilities_getWhiteBalanceMode(AttachCurrentThread(),
                                                 object_));
}

PhotoCapabilities::AndroidFillLightMode PhotoCapabilities::getFillLightMode()
    const {
  DCHECK(!object_.is_null());
  return static_cast<AndroidFillLightMode>(
      Java_PhotoCapabilities_getFillLightMode(AttachCurrentThread(), object_));
}

bool PhotoCapabilities::getRedEyeReduction() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getRedEyeReduction(AttachCurrentThread(),
                                                   object_);
}

int PhotoCapabilities::getMinColorTemperature() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinColorTemperature(AttachCurrentThread(),
                                                       object_);
}

int PhotoCapabilities::getMaxColorTemperature() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxColorTemperature(AttachCurrentThread(),
                                                       object_);
}

int PhotoCapabilities::getCurrentColorTemperature() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentColorTemperature(
      AttachCurrentThread(), object_);
}

}  // namespace media
