// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RLZ_REVENUE_STATS_H_
#define CHROME_BROWSER_ANDROID_RLZ_REVENUE_STATS_H_

#include <jni.h>

namespace chrome {
namespace android {

bool RegisterRevenueStats(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_RLZ_REVENUE_STATS_H_
