// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "jni/DomDistillerUrlUtils_jni.h"
#include "net/base/url_util.h"
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

jstring GetOriginalUrlFromDistillerUrl(JNIEnv* env,
                                       jclass clazz,
                                       jstring j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid())
    return NULL;

  std::string original_url_str;
  net::GetValueForKeyInQuery(url, kUrlKey, &original_url_str);
  GURL original_url(original_url_str);
  if (!original_url.is_valid())
    return NULL;

  return base::android::ConvertUTF8ToJavaString(env, original_url.spec())
      .Release();
}

jboolean IsDistilledPage(JNIEnv* env, jclass clazz, jstring j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  return dom_distiller::url_utils::IsDistilledPage(url);
}

jboolean IsUrlDistillable(JNIEnv* env, jclass clazz, jstring j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  return dom_distiller::url_utils::IsUrlDistillable(url);
}

jstring GetIsDistillableJs(JNIEnv* env, jclass clazz) {
  return base::android::ConvertUTF8ToJavaString(
      env, dom_distiller::url_utils::GetIsDistillableJs()).Release();
}

bool RegisterUrlUtils(JNIEnv* env) { return RegisterNativesImpl(env); }

}  // namespace android

}  // namespace url_utils

}  // namespace dom_distiller
