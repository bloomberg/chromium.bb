// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "components/enhanced_bookmarks/android/enhanced_bookmarks_bridge.h"

using base::android::RegistrationMethod;

namespace enhanced_bookmarks {
namespace android {

static RegistrationMethod kEnhancedBookmarksRegisteredMethods[] = {
    {"EnhancedBookmarksBridge", RegisterEnhancedBookmarksBridge},
};

bool RegisterEnhancedBookmarks(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kEnhancedBookmarksRegisteredMethods,
      arraysize(kEnhancedBookmarksRegisteredMethods));
}

}  // namespace android
}  // namespace enhanced_bookmarks
