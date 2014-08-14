// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_

#include "base/android/jni_android.h"

class BookmarkModel;

namespace enhanced_bookmarks {
namespace android {

class EnhancedBookmarksBridge {
 public:
  EnhancedBookmarksBridge(JNIEnv* env, jobject obj, jlong bookmark_model_ptr);
  void Destroy(JNIEnv*, jobject);

  base::android::ScopedJavaLocalRef<jstring> GetBookmarkDescription(
      JNIEnv* env,
      jobject obj,
      jlong id,
      jint type);

  void SetBookmarkDescription(JNIEnv* env,
                              jobject obj,
                              jlong id,
                              jint type,
                              jstring description);

 private:
  BookmarkModel* bookmark_model_;  // weak

  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarksBridge);
};

bool RegisterEnhancedBookmarksBridge(JNIEnv* env);

}  // namespace android
}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
