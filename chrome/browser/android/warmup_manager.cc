// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/render_process_host.h"
#include "jni/WarmupManager_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

static void PreconnectUrlAndSubresources(JNIEnv* env,
                                         const JavaParamRef<jclass>& clazz,
                                         const JavaParamRef<jobject>& jprofile,
                                         const JavaParamRef<jstring>& url_str) {
  if (url_str) {
    GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, url_str));
    Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
    if (profile) {
      profile->GetNetworkPredictor()->PreconnectUrlAndSubresources(url, GURL());
    }
  }
}

static void WarmupSpareRenderer(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (profile) {
    content::RenderProcessHost::WarmupSpareRenderProcessHost(profile);
  }
}
