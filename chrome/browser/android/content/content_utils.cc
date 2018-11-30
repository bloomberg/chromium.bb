// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "jni/ContentUtils_jni.h"

static base::android::ScopedJavaLocalRef<jstring>
JNI_ContentUtils_GetBrowserUserAgent(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetUserAgent());
}

static void JNI_ContentUtils_SetUserAgentOverride(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  ChromeContentClient content_client;
  std::string product = content_client.GetProduct();
  std::string spoofed_ua =
      content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  web_contents->SetUserAgentOverride(spoofed_ua, false);
}
