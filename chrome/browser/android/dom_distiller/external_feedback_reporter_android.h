// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOM_DISTILLER_EXTERNAL_FEEDBACK_REPORTER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DOM_DISTILLER_EXTERNAL_FEEDBACK_REPORTER_ANDROID_H_

#include "components/dom_distiller/core/external_feedback_reporter.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

namespace android {

class ExternalFeedbackReporterAndroid : public ExternalFeedbackReporter {
 public:
  ExternalFeedbackReporterAndroid() {}
  ~ExternalFeedbackReporterAndroid() override {}

  // ExternalFeedbackReporter implementation.
  void ReportExternalFeedback(content::WebContents* web_contents,
                              const GURL& url,
                              const bool good) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalFeedbackReporterAndroid);
};

// Registers the FeedbackReporter's native methods through JNI.
bool RegisterFeedbackReporter(JNIEnv* env);

}  // namespace android

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_ANDROID_DOM_DISTILLER_EXTERNAL_FEEDBACK_REPORTER_ANDROID_H_
