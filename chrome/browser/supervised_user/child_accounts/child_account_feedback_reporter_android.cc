// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChildAccountFeedbackReporter_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

void ReportChildAccountFeedback(content::WebContents* web_contents,
                                const std::string& description,
                                const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();

  WindowAndroidHelper* helper =
      content::WebContentsUserData<WindowAndroidHelper>::FromWebContents(
          web_contents);
  ui::WindowAndroid* window = helper->GetWindowAndroid();

  ScopedJavaLocalRef<jstring> jdesc =
      base::android::ConvertUTF8ToJavaString(env, description);
  ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  Java_ChildAccountFeedbackReporter_reportFeedbackWithWindow(
      env, window->GetJavaObject().obj(), jdesc.obj(), jurl.obj());
}

bool RegisterChildAccountFeedbackReporter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
