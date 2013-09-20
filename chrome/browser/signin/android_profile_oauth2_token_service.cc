// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android_profile_oauth2_token_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AndroidProfileOAuth2TokenServiceHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

std::string CombineScopes(const OAuth2TokenService::ScopeSet& scopes) {
  // The Android AccountManager supports multiple scopes separated by a space:
  // https://code.google.com/p/google-api-java-client/wiki/OAuth2#Android
  std::string scope;
  for (OAuth2TokenService::ScopeSet::const_iterator it = scopes.begin();
       it != scopes.end(); ++it) {
    if (!scope.empty())
      scope += " ";
    scope += *it;
  }
  return scope;
}

// Callback from FetchOAuth2TokenWithUsername().
// Arguments:
// - the error, or NONE if the token fetch was successful.
// - the OAuth2 access token.
// - the expiry time of the token (may be null, indicating that the expiry
//   time is unknown.
typedef base::Callback<void(
    const GoogleServiceAuthError&, const std::string&, const base::Time&)>
        FetchOAuth2TokenCallback;

}  // namespace

AndroidProfileOAuth2TokenService::AndroidProfileOAuth2TokenService() {
}

AndroidProfileOAuth2TokenService::~AndroidProfileOAuth2TokenService() {
}

bool AndroidProfileOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_id =
      ConvertUTF8ToJavaString(env, account_id);
  jboolean refresh_token_is_available =
      Java_AndroidProfileOAuth2TokenServiceHelper_hasOAuth2RefreshToken(
          env, base::android::GetApplicationContext(),
          j_account_id.obj());
  return refresh_token_is_available != JNI_FALSE;
}

std::vector<std::string> AndroidProfileOAuth2TokenService::GetAccounts() {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_AndroidProfileOAuth2TokenServiceHelper_getAccounts(
          env, base::android::GetApplicationContext());
  // TODO(fgorski): We may decide to filter out some of the accounts.
  base::android::AppendJavaStringArrayToStringVector(env,
                                                     j_accounts.obj(),
                                                     &accounts);
  return accounts;
}

void AndroidProfileOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!account_id.empty());

  JNIEnv* env = AttachCurrentThread();
  std::string scope = CombineScopes(scopes);
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF8ToJavaString(env, account_id);
  ScopedJavaLocalRef<jstring> j_scope =
      ConvertUTF8ToJavaString(env, scope);

  // Allocate a copy of the request WeakPtr on the heap, because the object
  // needs to be passed through JNI as an int.
  // It will be passed back to OAuth2TokenFetched(), where it will be freed.
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(base::Bind(&RequestImpl::InformConsumer,
                                              request->AsWeakPtr())));

  // Call into Java to get a new token.
  Java_AndroidProfileOAuth2TokenServiceHelper_getOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_username.obj(),
      j_scope.obj(),
      reinterpret_cast<int>(heap_callback.release()));
}

void AndroidProfileOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  OAuth2TokenService::InvalidateOAuth2Token(account_id,
                                            client_id,
                                            scopes,
                                            access_token);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_access_token =
      ConvertUTF8ToJavaString(env, access_token);
  Java_AndroidProfileOAuth2TokenServiceHelper_invalidateOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_access_token.obj());
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void OAuth2TokenFetched(JNIEnv* env, jclass clazz,
    jstring authToken,
    jboolean result,
    jint nativeCallback) {
  std::string token = ConvertJavaStringToUTF8(env, authToken);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(nativeCallback));
  GoogleServiceAuthError err(result ?
                             GoogleServiceAuthError::NONE :
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  heap_callback->Run(err, token, base::Time());
}

// static
bool AndroidProfileOAuth2TokenService::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
