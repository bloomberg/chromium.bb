// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_

#include "base/android/scoped_java_ref.h"

namespace feed {

// Native counterpart of FeedLoggingBridge.java. Holds non-owning pointers
// to native implementation, to which operations are delegated. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedLoggingBridge {
 public:
  FeedLoggingBridge();
  ~FeedLoggingBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void OnContentViewed(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const jint position,
                       const jlong publishedTimeSeconds,
                       const jlong timeContentBecameAvailable,
                       const jfloat score);

  void OnContentDismissed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const base::android::JavaRef<jstring>& j_url);

  void OnContentClicked(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const jint position,
                        const jlong publishedTimeSeconds,
                        const jfloat score);

  void OnClientAction(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jint j_action_type);

  void OnContentContextMenuOpened(JNIEnv* j_env,
                                  const base::android::JavaRef<jobject>& j_this,
                                  const jint position,
                                  const jlong publishedTimeSeconds,
                                  const jfloat score);

  void OnMoreButtonViewed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jint j_position);

  void OnMoreButtonClicked(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jint j_position);

  void OnOpenedWithContent(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jint j_time_to_Populate,
                           const jint j_content_count);

  void OnOpenedWithNoImmediateContent(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this);

  void OnOpenedWithNoContent(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedLoggingBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
