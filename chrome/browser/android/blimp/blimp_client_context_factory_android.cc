// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/blimp_client_context_factory_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/public/blimp_client_context.h"
#include "chrome/browser/android/blimp/blimp_client_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/BlimpClientContextFactory_jni.h"

using base::android::JavaParamRef;

namespace content {
class BrowserContext;
}

static base::android::ScopedJavaLocalRef<jobject>
GetBlimpClientContextForProfile(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  return blimp::client::BlimpClientContext::GetJavaObject(
      BlimpClientContextFactory::GetInstance()->GetForBrowserContext(profile));
}

bool RegisterBlimpClientContextFactoryJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
