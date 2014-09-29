// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARKS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARKS_BRIDGE_H_

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace enhanced_bookmarks {
namespace android {

class EnhancedBookmarksBridge {
 public:
  EnhancedBookmarksBridge(JNIEnv* env, jobject obj, Profile* profile);
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
  Profile* profile_;                       // weak
  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarksBridge);
};

bool RegisterEnhancedBookmarksBridge(JNIEnv* env);

}  // namespace android
}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ANDROID_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARKS_BRIDGE_H_
