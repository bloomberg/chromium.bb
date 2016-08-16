// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_promo_util_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "jni/SigninPromoUtil_jni.h"
#include "ui/android/window_android.h"

namespace chrome {
namespace android {

// static
void SigninPromoUtilAndroid::StartAccountSigninActivityForPromo(
    content::ContentViewCore* content_view_core,
    signin_metrics::AccessPoint access_point) {
  if (content_view_core && content_view_core->GetWindowAndroid()) {
    Java_SigninPromoUtil_openAccountSigninActivityForPromo(
        base::android::AttachCurrentThread(),
        content_view_core->GetWindowAndroid()->GetJavaObject(),
        jint(access_point));
  }
}

}  // namespace android
}  // namespace chrome
