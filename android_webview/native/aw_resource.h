// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_RESOURCE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_RESOURCE_H_

#include "base/android/jni_android.h"

#include <string>

namespace android_webview {
namespace AwResource {

bool RegisterAwResource(JNIEnv* env);

}  // namespace AwResource
}  // namsespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_RESOURCE_H_
