// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dom_distiller/external_feedback_reporter_android.h"

#include "chrome/browser/android/dom_distiller/feedback_reporter_android.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

namespace android {

// ExternalFeedbackReporter implementation.
void ExternalFeedbackReporterAndroid::ReportExternalFeedback(
    content::WebContents* web_contents,
    const GURL& url,
    const bool good) {
  ReportDomDistillerExternalFeedback(web_contents, url, good);
}

}  // namespace android

}  // namespace dom_distiller
