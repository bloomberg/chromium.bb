// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils_android.h"

#include <string>

#include "chrome/browser/dom_distiller/tab_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/DomDistillerTabUtils_jni.h"

namespace android {

void DistillCurrentPageAndView(JNIEnv* env,
                               jclass clazz,
                               jobject j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::DistillCurrentPageAndView(web_contents);
}

}  // namespace android

bool RegisterDomDistillerTabUtils(JNIEnv* env) {
  return android::RegisterNativesImpl(env);
}
