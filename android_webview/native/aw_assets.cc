// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "android_webview/native/aw_assets.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/AwAssets_jni.h"

namespace android_webview {
namespace AwAssets {

bool OpenAsset(const std::string& filename,
               int* fd,
               int64* offset,
               int64* size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jarr = Java_AwAssets_openAsset(
      env,
      base::android::GetApplicationContext(),
      base::android::ConvertUTF8ToJavaString(env, filename).Release());
  std::vector<jlong> results;
  base::android::JavaLongArrayToLongVector(env, jarr.obj(), &results);
  DCHECK_EQ(3U, results.size());
  *fd = static_cast<int>(results[0]);
  *offset = results[1];
  *size = results[2];
  return *fd != -1;
}

bool RegisterAssetWithGlobalDescriptors(base::GlobalDescriptors::Key key,
                                        const std::string& asset_filename) {
  int asset_fd = 0;
  int64 asset_off = 0;
  int64 asset_len = 0;
  bool result =
      AwAssets::OpenAsset(asset_filename, &asset_fd, &asset_off, &asset_len);
  if (result) {
    base::GlobalDescriptors::GetInstance()->Set(
        key, asset_fd, base::MemoryMappedFile::Region(asset_off, asset_len));
  }
  return result;
}

}  // namespace AwAssets

bool RegisterAwAssets(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
