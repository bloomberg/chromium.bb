// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "jni/PrefetchTestBridge_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Below is the native implementation of PrefetchTestBridge.java.

namespace offline_pages {
namespace prefetch {
JNI_EXPORT void JNI_PrefetchTestBridge_AddSuggestedURLs(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobjectArray>& urls) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  CHECK(profile);
  PrefetchService* service =
      PrefetchServiceFactory::GetForBrowserContext(profile);
  CHECK(service);
  std::vector<PrefetchURL> suggestions;
  for (jsize i = 0; i < env->GetArrayLength(urls); ++i) {
    ScopedJavaLocalRef<jstring> url(
        env, (jstring)env->GetObjectArrayElement(urls, i));
    std::string curl = base::android::ConvertJavaStringToUTF8(url);
    suggestions.push_back(PrefetchURL(curl, GURL(curl), base::string16()));
  }

  service->GetPrefetchDispatcher()->AddCandidatePrefetchURLs(
      offline_pages::kSuggestedArticlesNamespace, suggestions);
}

JNI_EXPORT void JNI_PrefetchTestBridge_EnableLimitlessPrefetching(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    jboolean enable) {
  SetLimitlessPrefetchingEnabledForTesting(enable != 0);
}

JNI_EXPORT jboolean JNI_PrefetchTestBridge_IsLimitlessPrefetchingEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  return static_cast<jboolean>(IsLimitlessPrefetchingEnabled());
}

}  // namespace prefetch
}  // namespace offline_pages
