// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "content/public/browser/android/content_view_core.h"

namespace chrome {
namespace android {

class SigninPromoUtilAndroid {
 public:
  // Opens a signin flow with the specified |access_point| for metrics within
  // the context of |content_view_core|, which could be null.
  static void StartAccountSigninActivityForPromo(
      content::ContentViewCore* content_view_core,
      signin_metrics::AccessPoint access_point);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_
