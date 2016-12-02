// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// Alphabetical:
extern const base::Feature kAndroidPayIntegrationV1;
extern const base::Feature kCCTExternalLinkHandling;
extern const base::Feature kCCTPostMessageAPI;
extern const base::Feature kChromeHomeFeature;
extern const base::Feature kContextualSearchSingleActions;
extern const base::Feature kDownloadsUiFeature;
extern const base::Feature kImportantSitesInCBD;
extern const base::Feature kNoCreditCardAbort;
extern const base::Feature kNTPFakeOmniboxTextFeature;
extern const base::Feature kNTPOfflinePagesFeature;
extern const base::Feature kPhysicalWebFeature;
extern const base::Feature kPhysicalWebIgnoreOtherClientsFeature;
extern const base::Feature kSpecialLocaleFeature;
extern const base::Feature kSpecialLocaleWrapper;
extern const base::Feature kTabReparenting;
extern const base::Feature kUserMediaScreenCapturing;
extern const base::Feature kWebApks;
extern const base::Feature kWebPaymentsModifiers;

bool RegisterChromeFeatureListJni(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_FEATURE_LIST_H_
