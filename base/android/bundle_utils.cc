// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/bundle_utils.h"

#include <dlfcn.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/BundleUtils_jni.h"

namespace base {
namespace android {

// static
bool BundleUtils::IsBundle() {
  return Java_BundleUtils_isBundle(base::android::AttachCurrentThread());
}

// static
void* BundleUtils::DlOpenModuleLibrary(const std::string& libary_name) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_path = Java_BundleUtils_getNativeLibraryPath(
      env, base::android::ConvertUTF8ToJavaString(env, libary_name));
  std::string library_path =
      base::android::ConvertJavaStringToUTF8(env, java_path);
  return dlopen(library_path.c_str(), RTLD_LOCAL | RTLD_NOW);
}

}  // namespace android
}  // namespace base
