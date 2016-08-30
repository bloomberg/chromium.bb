// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_H_
#define CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_H_

#include <stdint.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context_getter.h"

class Profile;

// This class can be used to retrieve an array of cookies from the cookie jar
// as well as insert an array of cookies into it. This class is the underlying
// glue that interacts with CookiesFetch.java and its lifetime is governed by
// the Java counter part.
class CookiesFetcher {
 public:
  // Constructs a fetcher that can interact with the cookie jar in the
  // specified profile.
  explicit CookiesFetcher(JNIEnv* env, jobject obj, Profile* profile);

  ~CookiesFetcher();

  // Fetches all cookies from the cookie jar.
  void PersistCookies(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);

 private:
  void PersistCookiesInternal(net::URLRequestContextGetter* getter);

  // Callback used after the cookie jar populate the cookie list for us.
  void OnCookiesFetchFinished(const net::CookieList& cookies);

  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  DISALLOW_COPY_AND_ASSIGN(CookiesFetcher);
};

// Registers the CookiesFetcher native method.
bool RegisterCookiesFetcher(JNIEnv* env);

#endif // CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_H_
