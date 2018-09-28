// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_logging_bridge.h"

#include <jni.h>

#include "jni/FeedLoggingBridge_jni.h"

namespace feed {

using base::android::JavaRef;
using base::android::JavaParamRef;

static jlong JNI_FeedLoggingBridge_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_this) {
  FeedLoggingBridge* native_logging_bridge = new FeedLoggingBridge();
  return reinterpret_cast<intptr_t>(native_logging_bridge);
}

FeedLoggingBridge::FeedLoggingBridge() = default;

FeedLoggingBridge::~FeedLoggingBridge() = default;

void FeedLoggingBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedLoggingBridge::OnOpenedWithContent(JNIEnv* j_env,
                                            const JavaRef<jobject>& j_this,
                                            const jint j_content_count) {}

}  // namespace feed
