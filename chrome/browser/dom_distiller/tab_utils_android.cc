// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "jni/DomDistillerTabUtils_jni.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

namespace android {

void DistillCurrentPageAndView(JNIEnv* env,
                               jclass clazz,
                               jobject j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::DistillCurrentPageAndView(web_contents);
}

void DistillAndView(JNIEnv* env,
                    jclass clazz,
                    jobject j_source_web_contents,
                    jobject j_destination_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  content::WebContents* destination_web_contents =
      content::WebContents::FromJavaWebContents(j_destination_web_contents);
  ::DistillAndView(source_web_contents, destination_web_contents);
}

jstring GetFormattedUrlFromOriginalDistillerUrl(JNIEnv* env,
                                                jclass clazz,
                                                jstring j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  Profile* profile = ProfileManager::GetLastUsedProfile();
  std::string languages;  // Empty if Profile cannot be retrieved.
  if (profile) {
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  }

  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return base::android::ConvertUTF16ToJavaString(
             env, url_formatter::FormatUrl(
                      url, languages, url_formatter::kFormatUrlOmitAll,
                      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr))
      .Release();
}

}  // namespace android

bool RegisterDomDistillerTabUtils(JNIEnv* env) {
  return android::RegisterNativesImpl(env);
}
