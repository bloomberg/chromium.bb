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

  void OnOpenedWithContent(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jint j_content_count);

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedLoggingBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
