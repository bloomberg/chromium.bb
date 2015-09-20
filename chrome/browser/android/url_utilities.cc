// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/google/core/browser/google_util.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "jni/UrlUtilities_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;

namespace {

GURL ConvertJavaStringToGURL(JNIEnv*env, jstring url) {
  return url ? GURL(ConvertJavaStringToUTF8(env, url)) : GURL();
}

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    jboolean include_private) {
  return include_private
      ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
      : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

static jboolean SameDomainOrHost(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jstring>& url_1_str,
                                 const JavaParamRef<jstring>& url_2_str,
                                 jboolean include_private) {
  GURL url_1 = ConvertJavaStringToGURL(env, url_1_str);
  GURL url_2 = ConvertJavaStringToGURL(env, url_2_str);

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1,
                                                            url_2,
                                                            filter);
}

static ScopedJavaLocalRef<jstring> GetDomainAndRegistry(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url,
    jboolean include_private) {
  DCHECK(url);
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return ScopedJavaLocalRef<jstring>();

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return base::android::ConvertUTF8ToJavaString(
      env,
      net::registry_controlled_domains::GetDomainAndRegistry(gurl, filter));
}

static jboolean IsGoogleSearchUrl(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jstring>& url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleSearchUrl(gurl);
}

static ScopedJavaLocalRef<jstring> FormatUrlForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               ConvertJavaStringToGURL(env, url), std::string()));
}

static ScopedJavaLocalRef<jstring> FormatUrlForSecurityDisplayOmitScheme(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplayOmitScheme(
               ConvertJavaStringToGURL(env, url), std::string()));
}

static jboolean IsGoogleHomePageUrl(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleHomePageUrl(gurl);
}

static ScopedJavaLocalRef<jstring> FixupUrl(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& optional_desired_tld) {
  DCHECK(url);
  GURL fixed_url = url_formatter::FixupURL(
      base::android::ConvertJavaStringToUTF8(env, url),
      optional_desired_tld
          ? base::android::ConvertJavaStringToUTF8(env, optional_desired_tld)
          : std::string());

  return fixed_url.is_valid()
             ? base::android::ConvertUTF8ToJavaString(env, fixed_url.spec())
             : ScopedJavaLocalRef<jstring>();
}

// Register native methods
bool RegisterUrlUtilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
