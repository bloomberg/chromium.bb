// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/android/cookies/cookies_fetcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "jni/CookiesFetcher_jni.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

CookiesFetcher::CookiesFetcher(JNIEnv* env, jobject obj, Profile* profile) {
}

CookiesFetcher::~CookiesFetcher() {
}

void CookiesFetcher::PersistCookies(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile->HasOffTheRecordProfile()) {
    // There is no work to be done. We might consider calling
    // the Java callback if needed.
    return;
  }
  profile = profile->GetOffTheRecordProfile();

  scoped_refptr<net::URLRequestContextGetter> getter(
      profile->GetRequestContext());

  // Take a strong global reference to the Java counter part and hold
  // on to it. This ensures the object would not be GC'd so we can call
  // the Java callback.
  jobject_.Reset(env, obj);

  // The rest must be done from the IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesFetcher::PersistCookiesInternal,
                 base::Unretained(this), base::RetainedRef(getter)));
}

void CookiesFetcher::PersistCookiesInternal(
    net::URLRequestContextGetter* getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieStore* store = getter->GetURLRequestContext()->cookie_store();

  // Nullable sometimes according to docs. There is no work need to be done
  // but we can consider calling the Java callback with empty output.
  if (!store) {
    delete this;
    return;
  }

  store->GetAllCookiesAsync(
      base::Bind(&CookiesFetcher::OnCookiesFetchFinished, base::Owned(this)));
}

void CookiesFetcher::OnCookiesFetchFinished(const net::CookieList& cookies) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> joa =
      Java_CookiesFetcher_createCookiesArray(env, jobject_, cookies.size());

  int index = 0;
  for (net::CookieList::const_iterator i = cookies.begin();
      i != cookies.end(); ++i) {
    std::string domain = i->Domain();
    if (domain.length() > 1 && domain[0] == '.')
      domain = domain.substr(1);
    ScopedJavaLocalRef<jobject> java_cookie = Java_CookiesFetcher_createCookie(
        env, jobject_, base::android::ConvertUTF8ToJavaString(env, i->Name()),
        base::android::ConvertUTF8ToJavaString(env, i->Value()),
        base::android::ConvertUTF8ToJavaString(env, i->Domain()),
        base::android::ConvertUTF8ToJavaString(env, i->Path()),
        i->CreationDate().ToInternalValue(), i->ExpiryDate().ToInternalValue(),
        i->LastAccessDate().ToInternalValue(), i->IsSecure(), i->IsHttpOnly(),
        static_cast<int>(i->SameSite()), i->Priority());
    env->SetObjectArrayElement(joa.obj(), index++, java_cookie.obj());
  }

  Java_CookiesFetcher_onCookieFetchFinished(env, jobject_, joa);
}

static void RestoreToCookieJarInternal(net::URLRequestContextGetter* getter,
                                       const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieStore* store = getter->GetURLRequestContext()->cookie_store();

  // Nullable sometimes according to docs.
  if (!store) {
    return;
  }

  base::Callback<void(bool success)> cb;

  // To re-create the original cookie, a domain should only be passed in to
  // SetCookieWithDetailsAsync if cookie.Domain() has a leading period, to
  // re-create the original cookie.
  std::string effective_domain = cookie.Domain();
  std::string host;
  if (effective_domain.length() > 1 && effective_domain[0] == '.') {
    host = effective_domain.substr(1);
  } else {
    host = effective_domain;
    effective_domain = "";
  }

  // Assume HTTPS - since the cookies are being restored from another store,
  // they have already gone through the strict secure check.
  GURL url(std::string(url::kHttpsScheme) + url::kStandardSchemeSeparator +
           host + "/");

  store->SetCookieWithDetailsAsync(
      url, cookie.Name(), cookie.Value(), effective_domain, cookie.Path(),
      base::Time(), cookie.ExpiryDate(), cookie.LastAccessDate(),
      cookie.IsSecure(), cookie.IsHttpOnly(), cookie.SameSite(),
      cookie.Priority(), cb);
}

static void RestoreCookies(JNIEnv* env,
                           const JavaParamRef<jclass>& jcaller,
                           const JavaParamRef<jstring>& name,
                           const JavaParamRef<jstring>& value,
                           const JavaParamRef<jstring>& domain,
                           const JavaParamRef<jstring>& path,
                           jlong creation,
                           jlong expiration,
                           jlong last_access,
                           jboolean secure,
                           jboolean httponly,
                           jint same_site,
                           jint priority) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile->HasOffTheRecordProfile()) {
    return;  // Don't create it. There is nothing to do.
  }
  profile = profile->GetOffTheRecordProfile();

  scoped_refptr<net::URLRequestContextGetter> getter(
      profile->GetRequestContext());

  net::CanonicalCookie cookie(*net::CanonicalCookie::Create(
      base::android::ConvertJavaStringToUTF8(env, name),
      base::android::ConvertJavaStringToUTF8(env, value),
      base::android::ConvertJavaStringToUTF8(env, domain),
      base::android::ConvertJavaStringToUTF8(env, path),
      base::Time::FromInternalValue(creation),
      base::Time::FromInternalValue(expiration),
      base::Time::FromInternalValue(last_access),
      secure, httponly,
      static_cast<net::CookieSameSite>(same_site),
      static_cast<net::CookiePriority>(priority)));

  // The rest must be done from the IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RestoreToCookieJarInternal, base::RetainedRef(getter),
                 cookie));
}

// JNI functions
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new CookiesFetcher(env, obj, 0));
}

// Register native methods
bool RegisterCookiesFetcher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
