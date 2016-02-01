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
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"

CookiesFetcher::CookiesFetcher(JNIEnv* env, jobject obj, Profile* profile) {
}

CookiesFetcher::~CookiesFetcher() {
}

void CookiesFetcher::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
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
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesFetcher::PersistCookiesInternal,
      base::Unretained(this), getter));
}

void CookiesFetcher::PersistCookiesInternal(
    net::URLRequestContextGetter* getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieStore* store = getter->GetURLRequestContext()->cookie_store();

  // Nullable sometimes according to docs. There is no work need to be done
  // but we can consider calling the Java callback with empty output.
  if (!store) {
    jobject_.Reset();
    return;
  }

  net::CookieMonster* monster = store->GetCookieMonster();

  monster->GetAllCookiesAsync(base::Bind(
      &CookiesFetcher::OnCookiesFetchFinished, base::Unretained(this)));
}

void CookiesFetcher::OnCookiesFetchFinished(const net::CookieList& cookies) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> joa =
      Java_CookiesFetcher_createCookiesArray(
          env, jobject_.obj(), cookies.size());

  int index = 0;
  for (net::CookieList::const_iterator i = cookies.begin();
      i != cookies.end(); ++i) {
    ScopedJavaLocalRef<jobject> java_cookie = Java_CookiesFetcher_createCookie(
        env, jobject_.obj(),
        base::android::ConvertUTF8ToJavaString(env, i->Source().spec()).obj(),
        base::android::ConvertUTF8ToJavaString(env, i->Name()).obj(),
        base::android::ConvertUTF8ToJavaString(env, i->Value()).obj(),
        base::android::ConvertUTF8ToJavaString(env, i->Domain()).obj(),
        base::android::ConvertUTF8ToJavaString(env, i->Path()).obj(),
        i->CreationDate().ToInternalValue(), i->ExpiryDate().ToInternalValue(),
        i->LastAccessDate().ToInternalValue(), i->IsSecure(), i->IsHttpOnly(),
        i->IsSameSite(), i->Priority());
    env->SetObjectArrayElement(joa.obj(), index++, java_cookie.obj());
  }

  Java_CookiesFetcher_onCookieFetchFinished(env, jobject_.obj(), joa.obj());

  // Give up the reference.
  jobject_.Reset();
}

void CookiesFetcher::RestoreCookies(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jstring>& url,
                                    const JavaParamRef<jstring>& name,
                                    const JavaParamRef<jstring>& value,
                                    const JavaParamRef<jstring>& domain,
                                    const JavaParamRef<jstring>& path,
                                    int64_t creation,
                                    int64_t expiration,
                                    int64_t last_access,
                                    bool secure,
                                    bool httponly,
                                    bool same_site,
                                    int priority) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile->HasOffTheRecordProfile()) {
      return; // Don't create it. There is nothing to do.
  }
  profile = profile->GetOffTheRecordProfile();

  scoped_refptr<net::URLRequestContextGetter> getter(
      profile->GetRequestContext());

  net::CanonicalCookie cookie(
      GURL(base::android::ConvertJavaStringToUTF8(env, url)),
      base::android::ConvertJavaStringToUTF8(env, name),
      base::android::ConvertJavaStringToUTF8(env, value),
      base::android::ConvertJavaStringToUTF8(env, domain),
      base::android::ConvertJavaStringToUTF8(env, path),
      base::Time::FromInternalValue(creation),
      base::Time::FromInternalValue(expiration),
      base::Time::FromInternalValue(last_access), secure, httponly, same_site,
      static_cast<net::CookiePriority>(priority));

  // The rest must be done from the IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CookiesFetcher::RestoreToCookieJarInternal,
                 base::Unretained(this),
                 getter,
                 cookie));
}

void CookiesFetcher::RestoreToCookieJarInternal(
    net::URLRequestContextGetter* getter,
    const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieStore* store = getter->GetURLRequestContext()->cookie_store();

  // Nullable sometimes according to docs.
  if (!store) {
      return;
  }

  net::CookieMonster* monster = store->GetCookieMonster();
  base::Callback<void(bool success)> cb;

  // TODO(estark): Remove kEnableExperimentalWebPlatformFeatures check
  // when we decide whether to ship cookie
  // prefixes. https://crbug.com/541511
  bool experimental_features_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  monster->SetCookieWithDetailsAsync(
      cookie.Source(), cookie.Name(), cookie.Value(), cookie.Domain(),
      cookie.Path(), cookie.ExpiryDate(), cookie.IsSecure(),
      cookie.IsHttpOnly(), cookie.IsSameSite(), experimental_features_enabled,
      experimental_features_enabled, cookie.Priority(), cb);
}

// JNI functions
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new CookiesFetcher(env, obj, 0));
}

// Register native methods
bool RegisterCookiesFetcher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
