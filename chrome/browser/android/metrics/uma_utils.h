// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_UMA_UTILS_H_
#define CHROME_BROWSER_ANDROID_METRICS_UMA_UTILS_H_

#include <jni.h>

#include "base/time/time.h"

namespace chrome {
namespace android {

base::TimeTicks GetMainEntryPointTimeTicks();

// Sets whether UMA reporting is enabled. This will call to Java to update
// the shared preference that is the source of truth for UMA reporting.
void SetUsageAndCrashReporting(bool enabled);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_METRICS_UMA_UTILS_H_
