// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

extern const base::Feature kImportantSitesInCBD;
extern const base::Feature kMediaStyleNotification;
extern const base::Feature kNTPMaterialDesign;
extern const base::Feature kNTPOfflinePagesFeature;
extern const base::Feature kNTPSnippetsFeature;
extern const base::Feature kNTPToolbarFeature;
extern const base::Feature kNTPTransparentDoodle;
extern const base::Feature kNTPFakeOmniboxTextFeature;
extern const base::Feature kPhysicalWebFeature;
extern const base::Feature kSystemDownloadManager;

bool RegisterChromeFeatureListJni(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
