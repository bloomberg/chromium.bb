// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOM_DISTILLER_FEEDBACK_REPORTER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DOM_DISTILLER_FEEDBACK_REPORTER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
}  // namespace content

namespace dom_distiller {

namespace android {

class FeedbackReporterAndroid : content::WebContentsObserver {
 public:
  FeedbackReporterAndroid(JNIEnv* env, jobject obj);
  virtual ~FeedbackReporterAndroid();

  // Destroys the FeedbackReporterAndroid.
  void Destroy(JNIEnv* env, jobject obj);

  // Observes a new WebContents, if necessary.
  void ReplaceWebContents(JNIEnv* env, jobject obj, jobject jweb_contents);

  // WebContentsObserver implementation:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // FeedbackReporterAndroid on the Java side.
  JavaObjectWeakGlobalRef weak_java_feedback_reporter_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackReporterAndroid);
};

// Registers the FeedbackReporter's native methods through JNI.
bool RegisterFeedbackReporter(JNIEnv* env);

}  // namespace android

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_ANDROID_DOM_DISTILLER_FEEDBACK_REPORTER_ANDROID_H_
