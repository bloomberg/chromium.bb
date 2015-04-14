// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dom_distiller/feedback_reporter_android.h"

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/common/chrome_switches.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/DomDistillerFeedbackReporter_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace android {

// static
jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  // TODO(mdjones): Remove android-view version of feedback overlay.
  return false;
}

// static
void ReportQuality(JNIEnv* env, jclass clazz, jboolean j_good) {
  FeedbackReporter::ReportQuality(j_good);
}

FeedbackReporterAndroid::FeedbackReporterAndroid(JNIEnv* env, jobject obj)
    : weak_java_feedback_reporter_(env, obj) {}

FeedbackReporterAndroid::~FeedbackReporterAndroid() {}

void FeedbackReporterAndroid::Destroy(JNIEnv* env, jobject obj) { delete this; }

void FeedbackReporterAndroid::ReplaceWebContents(JNIEnv* env,
                                                 jobject obj,
                                                 jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  Observe(web_contents);
}

void FeedbackReporterAndroid::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_feedback_reporter_.get(env);
  if (jobj.is_null())
    return;
  Java_DomDistillerFeedbackReporter_dismissOverlay(env, jobj.obj());
  GURL url = details.entry->GetURL();
  if (dom_distiller::url_utils::IsDistilledPage(url)) {
    Java_DomDistillerFeedbackReporter_showOverlay(env, jobj.obj());
  }
}

jlong Init(JNIEnv* env, jobject obj) {
  FeedbackReporterAndroid* reporter = new FeedbackReporterAndroid(env, obj);
  return reinterpret_cast<intptr_t>(reporter);
}

// static
void ReportDomDistillerExternalFeedback(content::WebContents* web_contents,
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
bool RegisterFeedbackReporter(JNIEnv* env) { return RegisterNativesImpl(env); }

}  // namespace android

}  // namespace dom_distiller
