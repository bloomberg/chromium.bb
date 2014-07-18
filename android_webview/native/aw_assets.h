// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_ASSETS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_ASSETS_H_

#include <string>

#include "base/android/jni_android.h"

namespace android_webview {
namespace AwAssets {

// Called by native to retrieve an asset (e.g. a .pak file) from the apk.
// Returns: true in case of success, false otherwise.
// Output arguments:
// - |fd|: file descriptor to the apk. The caller takes the ownership.
// - |offset|: offset in bytes from the start of the file
// - |size|: size in bytes of the asset / resource.
bool OpenAsset(const std::string& filename,
               int* fd,
               int64* offset,
               int64* size);

}  // namespace AwAssets

bool RegisterAwAssets(JNIEnv* env);

}  // namsespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_ASSETS_H_
