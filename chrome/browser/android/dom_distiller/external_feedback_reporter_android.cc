// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dom_distiller/external_feedback_reporter_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/DomDistillerFeedbackReporter_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace android {

// ExternalFeedbackReporter implementation.
void ExternalFeedbackReporterAndroid::ReportExternalFeedback(
    content::WebContents* web_contents,
    const GURL& url,
    const bool good) {
  if (!web_contents)
    return;
  WindowAndroidHelper* helper =
      content::WebContentsUserData<WindowAndroidHelper>::FromWebContents(
          web_contents);
  DCHECK(helper);

  ui::WindowAndroid* window = helper->GetWindowAndroid();
  DCHECK(window);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl = base::android::ConvertUTF8ToJavaString(
      env, url_utils::GetOriginalUrlFromDistillerUrl(url).spec());

  Java_DomDistillerFeedbackReporter_reportFeedbackWithWindow(
      env, window->GetJavaObject().obj(), jurl.obj(), good);
}

// static
bool RegisterFeedbackReporter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace dom_distiller
