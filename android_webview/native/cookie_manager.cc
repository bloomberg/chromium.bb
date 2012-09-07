// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/cookie_manager.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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

// This class is only available when building the chromium back-end for andriod
// webview: it is required where API backward compatibility demands that the UI
// thread must block waiting on other threads e.g. to obtain a synchronous
// return value. Long term, asynchronous overloads of all such methods will be
// added in the public API, and and no new uses of this will be allowed.
class ScopedAllowWaitForLegacyWebViewApi {
 private:
  base::ThreadRestrictions::ScopedAllowWait wait;
};

// CookieManager should be refactored to not require all tasks accessing the
// CookieStore to be piped through the IO thread.  It is currently required as
// the URLRequestContext provides the easiest mechanism for accessing the
// CookieStore, but the CookieStore is threadsafe.  In the future, we may
// instead want to inject an explicit CookieStore dependency into this object
// during process initialization to avoid depending on the URLRequestContext.
//
// In addition to the IO thread being the easiest access mechanism, it is also
// used because the async cookie tasks need to be processed on a different
// thread than the caller from Java, which can be any thread.  But, the calling
// thread will be 'dead' blocked waiting on the async task to complete, so we
// need it to complete on a 'live' (non-blocked) thread that is still pumping
// messages.
//
// We could refactor to only provide an asynchronous (but thread safe) API on
// the native side, and move this support for legacy synchronous blocking
// messages into a java-side worker thread.

namespace android_webview {

namespace {

typedef base::Callback<void(scoped_refptr<URLRequestContextGetter>,
                            base::WaitableEvent*)> CookieTask;

// Executes the |callback| task on the IO thread and |wait_for_completion|
// should only be true if the Java API method returns a value or is explicitly
// stated to be synchronous.
static void ExecCookieTask(const CookieTask& callback,
                           const bool wait_for_completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      android_webview::AwBrowserDependencyFactory::GetInstance()->
          GetBrowserContext(false);
  if (!context)
    return;

  scoped_refptr<URLRequestContextGetter> context_getter(
      context->GetRequestContext());

  if (wait_for_completion) {
    base::WaitableEvent completion(false, false);

    context->GetRequestContext()->GetNetworkTaskRunner()->PostTask(
        FROM_HERE, base::Bind(callback, context_getter, &completion));

    ScopedAllowWaitForLegacyWebViewApi wait;
    completion.Wait();
  } else {
    base::WaitableEvent* cb = NULL;
    context->GetRequestContext()->GetNetworkTaskRunner()->PostTask(
        FROM_HERE, base::Bind(callback, context_getter, cb));
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

// The CookieManager API does not return a value for SetCookie,
// so we don't need to propagate the |success| value back to the caller.
static void SetCookieCompleted(bool success) {
}

static void SetCookieAsyncHelper(
    const GURL& host,
    const std::string& value,
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {
  DCHECK(!completion);
  net::CookieOptions options;
  options.set_include_httponly();

  context_getter->GetURLRequestContext()->cookie_store()->
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
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {

  net::CookieOptions options;
  options.set_include_httponly();

  context_getter->GetURLRequestContext()->cookie_store()->
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

static void RemoveSessionCookieCompleted(int num_deleted) {
  // The CookieManager API does not return a value for removeSessionCookie,
  // so we don't need to propagate the |num_deleted| value back to the caller.
}

static void RemoveSessionCookieAsyncHelper(
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {
  DCHECK(!completion);
  net::CookieOptions options;
  options.set_include_httponly();

  CookieMonster* monster = context_getter->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  monster->DeleteSessionCookiesAsync(base::Bind(&RemoveSessionCookieCompleted));
}

}  // namespace

static void RemoveSessionCookie(JNIEnv* env, jobject obj) {
  ExecCookieTask(base::Bind(&RemoveSessionCookieAsyncHelper), false);
}

namespace {

static void RemoveAllCookieCompleted(int num_deleted) {
  // The CookieManager API does not return a value for removeAllCookie,
  // so we don't need to propagate the |num_deleted| value back to the caller.
}

static void RemoveAllCookieAsyncHelper(
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {
  DCHECK(!completion);
  CookieMonster* monster = context_getter->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  monster->DeleteAllAsync(base::Bind(&RemoveAllCookieCompleted));
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

static void HasCookiesAsyncHelper(
    bool* result,
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {

  CookieMonster* monster = context_getter->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  monster->GetAllCookiesAsync(base::Bind(&HasCookiesCompleted, completion,
                                         result));
}

}  // namespace

static jboolean HasCookies(JNIEnv* env, jobject obj) {
  bool has_cookies;
  ExecCookieTask(base::Bind(&HasCookiesAsyncHelper, &has_cookies), true);
  return has_cookies;
}

namespace {

static void AllowFileSchemeCookiesAsyncHelper(
    bool* accept,
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {

  CookieMonster* monster = context_getter->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  *accept = monster->IsCookieableScheme("file");

  DCHECK(completion);
  completion->Signal();
}

}  // namespace

static jboolean AllowFileSchemeCookies(JNIEnv* env, jclass obj) {
  bool accept;
  ExecCookieTask(base::Bind(&AllowFileSchemeCookiesAsyncHelper, &accept), true);
  return accept;
}

namespace {

static void SetAcceptFileSchemeCookiesAsyncHelper(
    bool accept,
    scoped_refptr<URLRequestContextGetter> context_getter,
    base::WaitableEvent* completion) {

  CookieMonster* monster = context_getter->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  monster->SetEnableFileScheme(accept);

  DCHECK(completion);
  completion->Signal();
}

}  // namespace

static void SetAcceptFileSchemeCookies(JNIEnv* env, jclass obj,
                                       jboolean accept) {
  ExecCookieTask(base::Bind(&SetAcceptFileSchemeCookiesAsyncHelper, accept),
                 true);
}

bool RegisterCookieManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // android_webview namespace
