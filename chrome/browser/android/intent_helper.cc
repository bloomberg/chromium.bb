// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/intent_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/IntentHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;

namespace chrome {
namespace android {

void SendEmail(const base::string16& d_email,
               const base::string16& d_subject,
               const base::string16& d_body,
               const base::string16& d_chooser_title,
               const base::string16& d_file_to_attach) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_email = ConvertUTF16ToJavaString(env, d_email);
  ScopedJavaLocalRef<jstring> j_subject =
      ConvertUTF16ToJavaString(env, d_subject);
  ScopedJavaLocalRef<jstring> j_body = ConvertUTF16ToJavaString(env, d_body);
  ScopedJavaLocalRef<jstring> j_chooser_title =
      ConvertUTF16ToJavaString(env, d_chooser_title);
  ScopedJavaLocalRef<jstring> j_file_to_attach =
      ConvertUTF16ToJavaString(env, d_file_to_attach);
  Java_IntentHelper_sendEmail(env,
                              base::android::GetApplicationContext(),
                              j_email.obj(),
                              j_subject.obj(),
                              j_body.obj(),
                              j_chooser_title.obj(),
                              j_file_to_attach.obj());
}

bool RegisterIntentHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
