// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/browser/ssl/connection_security_helper.h"
#include "content/public/browser/web_contents.h"

// static
jint GetSecurityLevelForWebContents(JNIEnv* env,
                                    jclass jcaller,
                                    jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  return ConnectionSecurityHelper::GetSecurityLevelForWebContents(web_contents);
}
