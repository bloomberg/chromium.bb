// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"

namespace media {

class PhotoCapabilities {
 public:
  explicit PhotoCapabilities(base::android::ScopedJavaLocalRef<jobject> object);
  ~PhotoCapabilities();

  static bool RegisterPhotoCapabilities(JNIEnv* env);

  int getMinIso() const;
  int getMaxIso() const;
  int getCurrentIso() const;
  int getMinHeight() const;
  int getMaxHeight() const;
  int getCurrentHeight() const;
  int getMinWidth() const;
  int getMaxWidth() const;
  int getCurrentWidth() const;
  int getMinZoom() const;
  int getMaxZoom() const;
  int getCurrentZoom() const;
  bool getAutoFocusInUse() const;

 private:
  const base::android::ScopedJavaLocalRef<jobject> object_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_
