// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/connection_security_android.h"

#include "base/logging.h"
#include "chrome/browser/ssl/connection_security.h"
#include "content/public/browser/web_contents.h"
#include "jni/ConnectionSecurity_jni.h"

// static
bool RegisterConnectionSecurityAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jint GetSecurityLevelForWebContents(JNIEnv* env,
                                    jclass jcaller,
                                    jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  return connection_security::GetSecurityLevelForWebContents(web_contents);
}
