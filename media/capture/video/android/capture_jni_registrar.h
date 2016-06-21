// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_CAPTURE_JNI_REGISTRAR_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_CAPTURE_JNI_REGISTRAR_H_

#include <jni.h>

#include "media/capture/capture_export.h"

namespace media {

// Register all JNI bindings necessary for capture.
CAPTURE_EXPORT bool RegisterCaptureJni(JNIEnv* env);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_CAPTURE_JNI_REGISTRAR_H_
