// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/feed/content/feed_offline_host.h"

namespace feed {

class FeedOfflineHost;

// Native counterpart of FeedOfflineBridge.java. Holds non-owning pointers to
// native implementation, to which operations are delegated. Also capable of
// calling back into Java half.
class FeedOfflineBridge {
 public:
  FeedOfflineBridge(const base::android::JavaRef<jobject>& j_this,
                    FeedOfflineHost* offline_host);
  ~FeedOfflineBridge();

  void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& j_this);

  base::android::ScopedJavaLocalRef<jobject> GetOfflineId(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_url);

  void GetOfflineStatus(JNIEnv* env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobjectArray>& j_urls,
                        const base::android::JavaRef<jobject>& j_callback);

  void OnContentRemoved(JNIEnv* env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobjectArray>& j_urls);

  void OnNewContentReceived(JNIEnv* env,
                            const base::android::JavaRef<jobject>& j_this);

  void OnNoListeners(JNIEnv* env,
                     const base::android::JavaRef<jobject>& j_this);

 private:
  // Reference to the Java half of this bridge. Always valid.
  base::android::ScopedJavaGlobalRef<jobject> j_this_;

  // Object to which all Java to native calls are delegated.
  FeedOfflineHost* offline_host_;

  DISALLOW_COPY_AND_ASSIGN(FeedOfflineBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_
