// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_
#define BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace blimp {
namespace client {

// The native component of org.chromium.blimp.input.WebInputBox.
class WebInputBox {
 public:
  static bool RegisterJni(JNIEnv* env);

  WebInputBox(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Brings up or hides the IME for user to enter text.
  void OnImeRequested(bool show);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Sends the text entered from IME to the blimp engine
  void OnImeTextEntered(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj,
                        const base::android::JavaParamRef<jstring>& text);

 private:
  virtual ~WebInputBox();

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(WebInputBox);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_
