// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "jni/PrefetchConfiguration_jni.h"

using base::android::JavaParamRef;

// These functions fulfill the Java to native link between
// PrefetchConfiguration.java and prefetch_prefs.

namespace offline_pages {
namespace android {

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsPrefetchingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  return profile &&
         static_cast<jboolean>(prefetch_prefs::IsEnabled(profile->GetPrefs()));
}

JNI_EXPORT void JNI_PrefetchConfiguration_SetPrefetchingEnabledInSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    jboolean enabled) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return;

  prefetch_prefs::SetPrefetchingEnabledInSettings(profile->GetPrefs(), enabled);
}

}  // namespace android
}  // namespace offline_pages
