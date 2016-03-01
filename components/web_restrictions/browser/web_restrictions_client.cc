// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_restrictions/browser/web_restrictions_client.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebRestrictionsClient_jni.h"

namespace web_restrictions {

namespace {

const size_t kMaxCacheSize = 100;

bool RequestPermissionTask(
    const GURL& url,
    const base::android::JavaRef<jobject>& java_provider) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebRestrictionsClient_requestPermission(
      env, java_provider.obj(),
      base::android::ConvertUTF8ToJavaString(env, url.spec()).obj());
}

bool CheckSupportsRequestTask(
    const base::android::JavaRef<jobject>& java_provider) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebRestrictionsClient_supportsRequest(env, java_provider.obj());
}

}  // namespace

// static
bool WebRestrictionsClient::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebRestrictionsClient::WebRestrictionsClient()
    : initialized_(false), supports_request_(false) {
  single_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  base::SequencedWorkerPool* worker_pool =
      content::BrowserThread::GetBlockingPool();
  background_task_runner_ =
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

WebRestrictionsClient::~WebRestrictionsClient() {
  if (java_provider_.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebRestrictionsClient_onDestroy(env, java_provider_.obj());
  java_provider_.Reset();
}

void WebRestrictionsClient::SetAuthority(
    const std::string& content_provider_authority) {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  // Destroy any existing content resolver.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_provider_.is_null()) {
    Java_WebRestrictionsClient_onDestroy(env, java_provider_.obj());
    java_provider_.Reset();
  }
  ClearCache();
  provider_authority_ = content_provider_authority;

  // Initialize the content resolver.
  initialized_ = !content_provider_authority.empty();
  if (!initialized_)
    return;
  java_provider_.Reset(Java_WebRestrictionsClient_create(
      env,
      base::android::ConvertUTF8ToJavaString(env, content_provider_authority)
          .obj(),
      reinterpret_cast<jlong>(this)));
  supports_request_ = false;
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&CheckSupportsRequestTask, java_provider_),
      base::Bind(&WebRestrictionsClient::RequestSupportKnown,
                 base::Unretained(this), provider_authority_));
}

UrlAccess WebRestrictionsClient::ShouldProceed(
    bool is_main_frame,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  if (!initialized_)
    return ALLOW;
  auto iter = url_access_cache_.find(url);
  if (iter != url_access_cache_.end()) {
    RecordURLAccess(url);
    return iter->second ? ALLOW : DISALLOW;
  }
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&WebRestrictionsClient::ShouldProceedTask, url,
                 java_provider_),
      base::Bind(&WebRestrictionsClient::OnShouldProceedComplete,
                 base::Unretained(this), provider_authority_, url, callback));

  return PENDING;
}

bool WebRestrictionsClient::SupportsRequest() const {
  return initialized_ && supports_request_;
}

bool WebRestrictionsClient::GetErrorHtml(const GURL& url,
                                         std::string* error_page) const {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  if (!initialized_)
    return false;
  auto iter = error_page_cache_.find(url);
  if (iter == error_page_cache_.end())
    return false;
  *error_page = iter->second;
  return true;
}

void WebRestrictionsClient::RequestPermission(
    const GURL& url,
    const base::Callback<void(bool)>& request_success) {
  if (!initialized_) {
    request_success.Run(false);
    return;
  }
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&RequestPermissionTask, url, java_provider_), request_success);
}

void WebRestrictionsClient::OnWebRestrictionsChanged() {
  single_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebRestrictionsClient::ClearCache, base::Unretained(this)));
}

void WebRestrictionsClient::RecordURLAccess(const GURL& url) {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  // Move the URL to the front of the cache.
  recent_urls_.remove(url);
  recent_urls_.push_front(url);
}

void WebRestrictionsClient::UpdateCache(std::string provider_authority,
                                        GURL url,
                                        bool should_proceed,
                                        std::string error_page) {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  // If the webrestrictions provider changed when the old one was being queried,
  // do not update the cache for the new provider.
  if (provider_authority != provider_authority_)
    return;
  RecordURLAccess(url);
  if (recent_urls_.size() >= kMaxCacheSize) {
    url_access_cache_.erase(recent_urls_.back());
    error_page_cache_.erase(recent_urls_.back());
    recent_urls_.pop_back();
  }
  url_access_cache_[url] = should_proceed;
  if (!error_page.empty()) {
    error_page_cache_[url] = error_page;
  } else {
    error_page_cache_.erase(url);
  }
}

void WebRestrictionsClient::RequestSupportKnown(std::string provider_authority,
                                                bool supports_request) {
  // |supports_request_| is initialized to false.
  DCHECK(!supports_request_);
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  // If the webrestrictions provider changed when the old one was being queried,
  // ignore the result.
  if (provider_authority != provider_authority_)
    return;
  supports_request_ = supports_request;
}

void WebRestrictionsClient::OnShouldProceedComplete(
    std::string provider_authority,
    const GURL& url,
    const base::Callback<void(bool)>& callback,
    const ShouldProceedResult& result) {
  UpdateCache(provider_authority, url, result.ok_to_proceed, result.error_page);
  callback.Run(result.ok_to_proceed);
}

void WebRestrictionsClient::ClearCache() {
  DCHECK(single_thread_task_runner_->BelongsToCurrentThread());
  error_page_cache_.clear();
  url_access_cache_.clear();
  recent_urls_.clear();
}

// static
WebRestrictionsClient::ShouldProceedResult
WebRestrictionsClient::ShouldProceedTask(
    const GURL& url,
    const base::android::JavaRef<jobject>& java_provider) {
  WebRestrictionsClient::ShouldProceedResult result;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_result =
      Java_WebRestrictionsClient_shouldProceed(
          env, java_provider.obj(),
          base::android::ConvertUTF8ToJavaString(env, url.spec()).obj());
  result.ok_to_proceed =
      Java_ShouldProceedResult_shouldProceed(env, j_result.obj());
  base::android::ScopedJavaLocalRef<jstring> j_error_page =
      Java_ShouldProceedResult_getErrorPage(env, j_result.obj());
  if (!j_error_page.is_null()) {
    base::android::ConvertJavaStringToUTF8(env, j_error_page.obj(),
                                           &result.error_page);
  }
  return result;
}

void NotifyWebRestrictionsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& clazz,
    jlong provider_ptr) {
  WebRestrictionsClient* provider =
      reinterpret_cast<WebRestrictionsClient*>(provider_ptr);
  // TODO(knn): Also reload existing interstitials/error pages.
  provider->OnWebRestrictionsChanged();
}

}  // namespace web_restrictions
