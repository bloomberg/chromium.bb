// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "jni/DomDistillerTabUtils_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android {

void DistillCurrentPageAndView(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz,
                               const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::DistillCurrentPageAndView(web_contents);
}

void DistillAndView(JNIEnv* env,
                    const JavaParamRef<jclass>& clazz,
                    const JavaParamRef<jobject>& j_source_web_contents,
                    const JavaParamRef<jobject>& j_destination_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  content::WebContents* destination_web_contents =
      content::WebContents::FromJavaWebContents(j_destination_web_contents);
  ::DistillAndView(source_web_contents, destination_web_contents);
}

ScopedJavaLocalRef<jstring> GetFormattedUrlFromOriginalDistillerUrl(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));

  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               url, url_formatter::kFormatUrlOmitAll,
               net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
}

// Returns true if the distiller experiment is set to use any heuristic other
// than "NONE". This is used to prevent the Reader Mode panel from loading
// when it would otherwise never be shown.
jboolean IsDistillerHeuristicsEnabled(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz) {
  return dom_distiller::GetDistillerHeuristicsType()
      != dom_distiller::DistillerHeuristicsType::NONE;
}

// Returns true if distiller is reporting every page as distillable.
jboolean IsHeuristicAlwaysTrue(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz) {
  return dom_distiller::GetDistillerHeuristicsType()
      == dom_distiller::DistillerHeuristicsType::ALWAYS_TRUE;
}

}  // namespace android

bool RegisterDomDistillerTabUtils(JNIEnv* env) {
  return android::RegisterNativesImpl(env);
}
