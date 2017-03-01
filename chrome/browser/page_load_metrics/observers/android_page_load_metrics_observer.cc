// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/PageLoadMetrics_jni.h"
#include "url/gurl.h"

AndroidPageLoadMetricsObserver::AndroidPageLoadMetricsObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void AndroidPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_contentful_paint_ms =
      timing.first_contentful_paint->InMilliseconds();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents_->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstContentfulPaint(
      env, java_web_contents,
      static_cast<jlong>(
          (extra_info.navigation_start - base::TimeTicks()).InMicroseconds()),
      static_cast<jlong>(first_contentful_paint_ms));
}

void AndroidPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t load_event_start_ms = timing.load_event_start->InMilliseconds();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents_->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onLoadEventStart(
      env, java_web_contents,
      static_cast<jlong>(
          (info.navigation_start - base::TimeTicks()).InMicroseconds()),
      static_cast<jlong>(load_event_start_ms));
}
