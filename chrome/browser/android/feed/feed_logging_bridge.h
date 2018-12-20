// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_

#include <utility>

#include "base/android/scoped_java_ref.h"

namespace feed {

class FeedLoggingMetrics;

// Native counterpart of FeedLoggingBridge.java. Holds non-owning pointers
// to native implementation, to which operations are delegated. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedLoggingBridge {
 public:
  explicit FeedLoggingBridge(FeedLoggingMetrics* feed_logging_metrics);
  ~FeedLoggingBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void OnContentViewed(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const jint j_position,
                       const jlong j_publishedTimeMs,
                       const jlong j_timeContentBecameAvailableMs,
                       const jfloat j_score);

  void OnContentDismissed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jint j_position,
                          const base::android::JavaRef<jstring>& j_url);

  void OnContentSwiped(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this);

  void OnClientAction(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jint j_window_open_disposition,
                      const jint j_position,
                      const jlong j_publishedTimeMs,
                      const jfloat j_score);

  void OnContentContextMenuOpened(JNIEnv* j_env,
                                  const base::android::JavaRef<jobject>& j_this,
                                  const jint j_position,
                                  const jlong j_publishedTimeMs,
                                  const jfloat j_score);

  void OnMoreButtonViewed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jint j_position);

  void OnMoreButtonClicked(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jint j_position);

  void OnOpenedWithContent(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jlong j_time_to_populate,
                           const jint j_content_count);

  void OnOpenedWithNoImmediateContent(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this);

  void OnOpenedWithNoContent(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);

  void OnSpinnerShown(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jlong j_shownTimeMs);

  void OnContentTargetVisited(JNIEnv* j_env,
                              const base::android::JavaRef<jobject>& j_this,
                              const jlong visit_time_ms,
                              const jboolean is_offline,
                              const jboolean return_to_ntp);

  void ReportScrolledAfterOpen(JNIEnv* j_env,
                               const base::android::JavaRef<jobject>& j_this);

 private:
  FeedLoggingMetrics* feed_logging_metrics_;

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
