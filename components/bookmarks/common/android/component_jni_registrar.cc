// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/bookmarks/common/android/bookmark_id.h"

namespace bookmarks {
namespace android {

static base::android::RegistrationMethod kBookmarksRegisteredMethods[] = {
    {"BookmarkId", RegisterBookmarkId},
};

bool RegisterBookmarks(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kBookmarksRegisteredMethods,
      arraysize(kBookmarksRegisteredMethods));
}

}  // namespace android
}  // namespace bookmarks
