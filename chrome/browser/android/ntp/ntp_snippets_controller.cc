// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_controller.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "jni/SnippetsController_jni.h"

using base::android::JavaParamRef;

static void FetchSnippets(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (profile) {
    ntp_snippets::NTPSnippetsService* ntp_snippets_service =
        NTPSnippetsServiceFactory::GetForProfile(profile);
    ntp_snippets_service->FetchSnippets();
  }
}

// static
bool NTPSnippetsController::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
