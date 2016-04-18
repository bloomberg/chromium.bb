// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_
#define CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_

#include <jni.h>

namespace chrome {
namespace android {

enum RunningModeHistogram {
  RUNNING_MODE_DOCUMENT_MODE,
  RUNNING_MODE_TABBED_MODE,
  RUNNING_MODE_MAX
};

enum CustomTabsVisibilityHistogram {
  VISIBLE_CUSTOM_TAB,
  VISIBLE_CHROME_TAB,
  CUSTOM_TABS_VISIBILITY_MAX
};

RunningModeHistogram GetDocumentModeValue();

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue();

bool GetIsInMultiWindowModeValue();

} // namespace android
} // namespace chrome

bool RegisterFeatureUtilities(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_
