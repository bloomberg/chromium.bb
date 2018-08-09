// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/unified_consent/unified_consent_service.h"
#include "jni/UnifiedConsentServiceBridge_jni.h"

using base::android::JavaParamRef;

static jboolean JNI_UnifiedConsentServiceBridge_ShouldShowConsentBump(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  return unifiedConsentService->ShouldShowConsentBump();
}
