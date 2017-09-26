// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "jni/BrowserSideNavigationPolicy_jni.h"

using base::android::JavaParamRef;

namespace content {

jboolean IsBrowserSideNavigationEnabled(JNIEnv* env,
                                        const JavaParamRef<jclass>& clazz) {
  return IsBrowserSideNavigationEnabled();
}

}  // namespace content
