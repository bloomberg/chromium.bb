// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_JNI_REGISTRAR_H_
#define MEDIA_BASE_ANDROID_MEDIA_JNI_REGISTRAR_H_

#include <jni.h>

#include "media/base/media_export.h"

namespace media {

// Register all JNI bindings necessary for media.
MEDIA_EXPORT bool RegisterJni(JNIEnv* env);

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_JNI_REGISTRAR_H_
