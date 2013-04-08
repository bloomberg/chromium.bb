// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include "base/logging.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/base/android/media_jni_registrar.h"
#endif

// This file is intended for platforms that don't need to load any media
// libraries (e.g., Android and iOS).
namespace media {

bool InitializeMediaLibrary(const base::FilePath& module_dir) {
  return true;
}

void InitializeMediaLibraryForTesting() {
#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  RegisterJni(env);
#endif
}

bool IsMediaLibraryInitialized() {
  return true;
}

}  // namespace media
