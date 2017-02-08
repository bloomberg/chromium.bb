// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_JNI_ONLOAD__H_
#define ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_JNI_ONLOAD__H_

#include <jni.h>

namespace android_webview {

// TODO(michaelbai): remove this once we no longer need to be able to run
// webview with manual JNI registration
bool OnJNIOnLoadRegisterJNI(JNIEnv* env);

bool OnJNIOnLoadInit();

}  // android_webview

#endif  // ANDROID_WEBVIEW_LIB_MAIN_WEBVIEW_JNI_ONLOAD__H_
