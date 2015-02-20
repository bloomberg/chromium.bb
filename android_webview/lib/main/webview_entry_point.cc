// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/webview_jni_onload_delegate.h"
#include "base/android/jni_android.h"
#include "content/public/app/content_jni_onload.h"

// This is called by the VM when the shared library is first loaded.
// Most of the initialization is done in LibraryLoadedOnMainThread(), not here.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // WebView uses native JNI exports; disable manual JNI registration to
  // improve startup peformance.
  base::android::DisableManualJniRegistration();

  if (!android_webview::OnJNIOnLoad(vm))
    return -1;

  return JNI_VERSION_1_4;
}
