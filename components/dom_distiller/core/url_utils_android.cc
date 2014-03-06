// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "components/dom_distiller/core/url_utils.h"
#include "jni/DomDistillerUrlUtils_jni.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace url_utils {

namespace android {

jstring GetDistillerViewUrlFromUrl(JNIEnv* env,
                                   jclass clazz,
                                   jstring j_scheme,
                                   jstring j_url) {
  std::string scheme(base::android::ConvertJavaStringToUTF8(env, j_scheme));
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid()) {
    return NULL;
  }
  GURL view_url =
      dom_distiller::url_utils::GetDistillerViewUrlFromUrl(scheme, url);
  if (!view_url.is_valid()) {
    return NULL;
  }
  return base::android::ConvertUTF8ToJavaString(env, view_url.spec()).Release();
}

bool RegisterUrlUtils(JNIEnv* env) { return RegisterNativesImpl(env); }

}  // namespace android

}  // namespace url_utils

}  // namespace dom_distiller
