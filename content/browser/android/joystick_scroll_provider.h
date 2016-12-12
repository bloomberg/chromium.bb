// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JOYSTICK_SCROLL_PROVIDER_H_
#define CONTENT_BROWSER_ANDROID_JOYSTICK_SCROLL_PROVIDER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"

namespace content {

class WebContentsImpl;

class JoystickScrollProvider {
 public:
  JoystickScrollProvider(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj,
                         WebContentsImpl* web_contents);

  ~JoystickScrollProvider();

  void ScrollBy(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jlong time_ms,
                jfloat dx_dip,
                jfloat dy_dip);

 private:
  class UserData;

  // A weak reference to the Java JoystickScrollProvider object.
  JavaObjectWeakGlobalRef java_ref_;

  WebContentsImpl* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(JoystickScrollProvider);
};

bool RegisterJoystickScrollProvider(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JOYSTICK_SCROLL_PROVIDER_H_
