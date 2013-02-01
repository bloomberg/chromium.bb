// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_JS_RESULT_HANDLER_H_
#define ANDROID_WEBVIEW_NATIVE_JS_RESULT_HANDLER_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace android_webview {

bool RegisterJsResultHandler(JNIEnv* env);

base::android::ScopedJavaLocalRef<jobject> createJsResultHandler(
    JNIEnv* env,
    const content::JavaScriptDialogManager::DialogClosedCallback*
        native_dialog_pointer);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_JS_RESULT_HANDLER_H_
