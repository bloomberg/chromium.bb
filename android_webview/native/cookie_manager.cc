// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/cookie_manager.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "jni/CookieManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using content::BrowserThread;
using net::CookieList;
using net::CookieMonster;
using net::URLRequestContextGetter;

// In the future, we may instead want to inject an explicit CookieStore
// dependency into this object during process initialization to avoid
// depending on the URLRequestContext.
// See issue http://crbug.com/157683

// All functions on the CookieManager can be called from any thread, including
// threads without a message loop. BrowserThread::FILE is used to call methods
// on CookieMonster that needs to be called, and called back, on a chrome
// thread.

namespace android_webview {

namespace {

typedef base::Callback<void(base::WaitableEvent*)> CookieTask;

class CookieManagerStatic {
 public:
  CookieManagerStatic() {
    // Get the context on the UI thread
    URLRequestContextGetter* context_getter_raw;
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetContext(&context_getter_raw, NULL);
    } else {
      base::WaitableEvent completion(false, false);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(GetContext, &context_getter_raw, &completion));
      ScopedAllowWaitForLegacyWebViewApi wait;
      completion.Wait();
    }
    scoped_refptr<URLRequestContextGetter> context_getter = context_getter_raw;

    // Get the URLRequestcontext (and CookieMonster) on the IO thread
    if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      GetCookieMonster(context_getter, &cookie_monster_, NULL);
    } else {
      base::WaitableEvent completion(false, false);
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          base::Bind(GetCookieMonster,
                     context_getter,
                     &cookie_monster_,
                     &completion));
      ScopedAllowWaitForLegacyWebViewApi wait;
      completion.Wait();
    }
  }

  CookieMonster* get_cookie_monster() {
    return cookie_monster_;
  }

 private:
  CookieMonster* cookie_monster_;

  // Must be called on UI thread
  static void GetContext(URLRequestContextGetter** context_getter,
                         base::WaitableEvent* completion) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    *context_getter = android_webview::AwBrowserDependencyFactory::
        GetInstance()->GetBrowserContext(false)->GetRequestContext();
    if (completion) {
      completion->Signal();
    }
  }

  // Must be called on the IO thread
  static void GetCookieMonster(
      scoped_refptr<URLRequestContextGetter> context_getter,
      CookieMonster** cookieMonster,
      base::WaitableEvent* completion) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    *cookieMonster = context_getter->GetURLRequestContext()->cookie_store()->
        GetCookieMonster();
    if (completion) {
      completion->Signal();
    }
  }
};

static base::LazyInstance<CookieManagerStatic> cookie_manager_static =
    LAZY_INSTANCE_INITIALIZER;

// Executes the |task| on the FILE thread. |wait_for_completion| should only be
// true if the Java API method returns a value or is explicitly stated to be
// synchronous.
static void ExecCookieTask(const CookieTask& task,
                           const bool wait_for_completion) {
  base::WaitableEvent completion(false, false);

  // Force construction of the wrapper as soon as possible. Also note that
  // constructing the cookie_manager_static inside the CookieTask will cause a
  // deadlock if the current thread is the UI or IO thread.
  cookie_manager_static.Get();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(task, wait_for_completion ? &completion : NULL));

  if (wait_for_completion) {
    ScopedAllowWaitForLegacyWebViewApi wait;
    completion.Wait();
  }
}

}  // namespace

static void SetAcceptCookie(JNIEnv* env, jobject obj, jboolean accept) {
  AwCookieAccessPolicy::GetInstance()->SetGlobalAllowAccess(accept);
}

static jboolean AcceptCookie(JNIEnv* env, jobject obj) {
  return AwCookieAccessPolicy::GetInstance()->GetGlobalAllowAccess();
}

namespace {

static void SetCookieCompleted(bool success) {
  // The CookieManager API does not return a value for SetCookie,
  // so we don't need to propagate the |success| value back to the caller.
}

static void SetCookieAsyncHelper(
    const GURL& host,
    const std::string& value,
    base::WaitableEvent* completion) {
  DCHECK(!completion);
  net::CookieOptions options;
  options.set_include_httponly();

  cookie_manager_static.Get().get_cookie_monster()->
      SetCookieWithOptionsAsync(host, value, options,
                                base::Bind(&SetCookieCompleted));
}

}  // namespace

static void SetCookie(JNIEnv* env, jobject obj, jstring url, jstring value) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));

  ExecCookieTask(base::Bind(&SetCookieAsyncHelper, host, cookie_value), false);
}

namespace {

static void GetCookieValueCompleted(base::WaitableEvent* completion,
                                    std::string* result,
                                    const std::string& value) {
  *result = value;
  DCHECK(completion);
  completion->Signal();
}

static void GetCookieValueAsyncHelper(
    const GURL& host,
    std::string* result,
    base::WaitableEvent* completion) {

  net::CookieOptions options;
  options.set_include_httponly();

  cookie_manager_static.Get().get_cookie_monster()->
      GetCookiesWithOptionsAsync(host, options,
                                 base::Bind(&GetCookieValueCompleted,
                                            completion,
                                            result));
}

}  // namespace

static jstring GetCookie(JNIEnv* env, jobject obj, jstring url) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value;
  ExecCookieTask(base::Bind(&GetCookieValueAsyncHelper, host, &cookie_value),
                 true);

  return base::android::ConvertUTF8ToJavaString(env, cookie_value).Release();
}

namespace {

static void RemoveCookiesCompleted(int num_deleted) {
  // The CookieManager API does not return a value for removeSessionCookie or
  // removeAllCookie, so we don't need to propagate the |num_deleted| value back
  // to the caller.
}

static void RemoveSessionCookieAsyncHelper(base::WaitableEvent* completion) {
  DCHECK(!completion);
  cookie_manager_static.Get().get_cookie_monster()->
      DeleteSessionCookiesAsync(base::Bind(&RemoveCookiesCompleted));
}

}  // namespace

static void RemoveSessionCookie(JNIEnv* env, jobject obj) {
  ExecCookieTask(base::Bind(&RemoveSessionCookieAsyncHelper), false);
}

namespace {

static void RemoveAllCookieAsyncHelper(base::WaitableEvent* completion) {
  DCHECK(!completion);
  cookie_manager_static.Get().get_cookie_monster()->
      DeleteAllAsync(base::Bind(&RemoveCookiesCompleted));
}

}  // namespace

static void RemoveAllCookie(JNIEnv* env, jobject obj) {
  ExecCookieTask(base::Bind(&RemoveAllCookieAsyncHelper), false);
}

static void RemoveExpiredCookie(JNIEnv* env, jobject obj) {
  // HasCookies will call GetAllCookiesAsync, which in turn will force a GC.
  HasCookies(env, obj);
}

namespace {

static void HasCookiesCompleted(base::WaitableEvent* completion,
                                bool* result,
                                const CookieList& cookies) {
  *result = cookies.size() != 0;
  DCHECK(completion);
  completion->Signal();
}

static void HasCookiesAsyncHelper(bool* result,
                                  base::WaitableEvent* completion) {
  cookie_manager_static.Get().get_cookie_monster()->
      GetAllCookiesAsync(base::Bind(&HasCookiesCompleted,
                                    completion,
                                    result));
}

}  // namespace

static jboolean HasCookies(JNIEnv* env, jobject obj) {
  bool has_cookies;
  ExecCookieTask(base::Bind(&HasCookiesAsyncHelper, &has_cookies), true);
  return has_cookies;
}

static jboolean AllowFileSchemeCookies(JNIEnv* env, jclass obj) {
  return cookie_manager_static.Get().get_cookie_monster()->
      IsCookieableScheme(chrome::kFileScheme);
}

static void SetAcceptFileSchemeCookies(JNIEnv* env, jclass obj,
                                       jboolean accept) {
  return cookie_manager_static.Get().get_cookie_monster()->
      SetEnableFileScheme(accept);
}

bool RegisterCookieManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // android_webview namespace
