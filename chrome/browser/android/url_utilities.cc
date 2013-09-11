// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/google/google_util.h"
#include "jni/UrlUtilities_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    jboolean include_private) {
  return include_private
      ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
      : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}

static jboolean SameDomainOrHost(JNIEnv* env,
                                 jclass clazz,
                                 jstring url_1_str,
                                 jstring url_2_str,
                                 jboolean include_private) {
  GURL url_1(base::android::ConvertJavaStringToUTF8(env, url_1_str));
  GURL url_2(base::android::ConvertJavaStringToUTF8(env, url_2_str));

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1,
                                                            url_2,
                                                            filter);
}

static jstring GetDomainAndRegistry(JNIEnv* env,
                                    jclass clazz,
                                    jstring url,
                                    jboolean include_private) {
  GURL gurl = GURL(base::android::ConvertJavaStringToUTF8(env, url));
  if (gurl.is_empty())
    return NULL;

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  // OK to release, JNI binding.
  return base::android::ConvertUTF8ToJavaString(
      env, net::registry_controlled_domains::GetDomainAndRegistry(
          gurl, filter)).Release();
}

static jboolean IsGoogleSearchUrl(JNIEnv* env, jclass clazz, jstring url) {
  GURL gurl = GURL(base::android::ConvertJavaStringToUTF8(env, url));
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleSearchUrl(gurl);
}

// Register native methods
bool RegisterUrlUtilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
