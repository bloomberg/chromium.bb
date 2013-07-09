// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android_profile_oauth2_token_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AndroidProfileOAuth2TokenServiceHelper_jni.h"
#include "net/url_request/url_request_context_getter.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

AndroidProfileOAuth2TokenService::AndroidProfileOAuth2TokenService(
    net::URLRequestContextGetter* getter)
    : ProfileOAuth2TokenService(getter) {
}

AndroidProfileOAuth2TokenService::~AndroidProfileOAuth2TokenService() {
}

scoped_ptr<OAuth2TokenService::Request>
    AndroidProfileOAuth2TokenService::StartRequest(
        const OAuth2TokenService::ScopeSet& scopes,
        OAuth2TokenService::Consumer* consumer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (HasCacheEntry(scopes))
    return StartCacheLookupRequest(scopes, consumer);

  scoped_ptr<RequestImpl> request(new RequestImpl(consumer));
  DCHECK_EQ(scopes.size(), 1U);
  std::vector<std::string> scope_list(scopes.begin(), scopes.end());
  FetchOAuth2Token(
      scope_list.front(),
      base::Bind(&RequestImpl::InformConsumer, request->AsWeakPtr()));
  return request.PassAs<Request>();
}

bool AndroidProfileOAuth2TokenService::RefreshTokenIsAvailable() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  return !signin_manager->GetAuthenticatedUsername().empty();
}

void AndroidProfileOAuth2TokenService::InvalidateToken(
    const ScopeSet& scopes,
    const std::string& invalid_token) {
  OAuth2TokenService::InvalidateToken(scopes, invalid_token);

  DCHECK_EQ(scopes.size(), 1U);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_invalid_token =
      ConvertUTF8ToJavaString(env, invalid_token);
  Java_AndroidProfileOAuth2TokenServiceHelper_invalidateOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_invalid_token.obj());
}

bool AndroidProfileOAuth2TokenService::ShouldCacheForRefreshToken(
    TokenService *token_service,
    const std::string& refresh_token) {
  // The parent class skips caching if the TokenService login token is stale,
  // but on Android the user is always logged in to exactly one profile, so
  // this concept doesn't exist and we can simply always cache.
  return true;
}

void AndroidProfileOAuth2TokenService::FetchOAuth2Token(
    const std::string& scope, const FetchOAuth2TokenCallback& callback) {
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile())->
          GetAuthenticatedUsername();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_sync_username =
      ConvertUTF8ToJavaString(env, sync_username);
  ScopedJavaLocalRef<jstring> j_scope =
      ConvertUTF8ToJavaString(env, scope);

  // Allocate a copy of the callback on the heap, because the callback
  // needs to be passed through JNI as an int.
  // It will be passed back to OAuth2TokenFetched(), where it will be freed.
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(callback));

  // Call into Java to get a new token.
  Java_AndroidProfileOAuth2TokenServiceHelper_getOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_sync_username.obj(),
      j_scope.obj(),
      reinterpret_cast<int>(heap_callback.release()));
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void OAuth2TokenFetched(JNIEnv* env, jclass clazz,
    jstring authToken,
    jboolean result,
    jint nativeCallback) {
  std::string token = ConvertJavaStringToUTF8(env, authToken);
  scoped_ptr<AndroidProfileOAuth2TokenService::FetchOAuth2TokenCallback>
      heap_callback(
          reinterpret_cast<
              AndroidProfileOAuth2TokenService::FetchOAuth2TokenCallback*>(
                  nativeCallback));
  GoogleServiceAuthError err(result ?
                             GoogleServiceAuthError::NONE :
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  heap_callback->Run(err, token, base::Time());
}

// static
bool AndroidProfileOAuth2TokenService::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
