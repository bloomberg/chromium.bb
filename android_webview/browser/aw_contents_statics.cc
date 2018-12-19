// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_safe_browsing_whitelist_manager.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "components/google/core/common/google_util.h"
#include "components/security_interstitials/core/urls.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "jni/AwContentsStatics_jni.h"
#include "net/cert/cert_database.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ClientCertificatesCleared(const JavaRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_clientCertificatesCleared(env, callback);
}

void NotifyClientCertificatesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();
}

void SafeBrowsingWhitelistAssigned(const JavaRef<jobject>& callback,
                                   bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_safeBrowsingWhitelistAssigned(env, callback, success);
}

}  // namespace

// static
ScopedJavaLocalRef<jstring>
JNI_AwContentsStatics_GetSafeBrowsingPrivacyPolicyUrl(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL privacy_policy_url(
      security_interstitials::kSafeBrowsingPrivacyPolicyUrl);
  std::string locale =
      AwBrowserContext::GetDefault()->GetSafeBrowsingUIManager()->app_locale();
  privacy_policy_url =
      google_util::AppendGoogleLocaleParam(privacy_policy_url, locale);
  return base::android::ConvertUTF8ToJavaString(env, privacy_policy_url.spec());
}

// static
void JNI_AwContentsStatics_ClearClientCertPreferences(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&NotifyClientCertificatesChanged),
      base::BindOnce(&ClientCertificatesCleared,
                     ScopedJavaGlobalRef<jobject>(env, callback)));
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContentsStatics_GetUnreachableWebDataUrl(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, content::kUnreachableWebDataURL);
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContentsStatics_GetProductVersion(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, version_info::GetVersionNumber());
}

// static
void JNI_AwContentsStatics_SetSafeBrowsingWhitelist(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jrules,
    const JavaParamRef<jobject>& callback) {
  std::vector<std::string> rules;
  base::android::AppendJavaStringArrayToStringVector(env, jrules, &rules);
  AwSafeBrowsingWhitelistManager* whitelist_manager =
      AwBrowserContext::GetDefault()->GetSafeBrowsingWhitelistManager();
  whitelist_manager->SetWhitelistOnUIThread(
      std::move(rules),
      base::BindOnce(&SafeBrowsingWhitelistAssigned,
                     ScopedJavaGlobalRef<jobject>(env, callback)));
}

// static
void JNI_AwContentsStatics_SetServiceWorkerIoThreadClient(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& io_thread_client,
    const base::android::JavaParamRef<jobject>& browser_context) {
  AwContentsIoThreadClient::SetServiceWorkerIoThreadClient(io_thread_client,
                                                           browser_context);
}

// static
void JNI_AwContentsStatics_SetCheckClearTextPermitted(
    JNIEnv* env,
    jboolean permitted) {
  AwURLRequestContextGetter::set_check_cleartext_permitted(permitted);
}

}  // namespace android_webview
