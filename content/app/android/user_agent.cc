// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/user_agent.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/user_agent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetApplicationContext;

namespace content {

std::string GetUserAgentOSInfo() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> os_info =
      Java_UserAgent_getUserAgentOSInfo(env);
  return ConvertJavaStringToUTF8(os_info);
}

bool RegisterUserAgent(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
