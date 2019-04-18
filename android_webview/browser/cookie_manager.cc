// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/cookie_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "jni/AwCookieManager_jni.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/url_constants.h"

using base::WaitableEvent;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using net::CookieList;

// In the future, we may instead want to inject an explicit CookieStore
// dependency into this object during process initialization to avoid
// depending on the URLRequestContext.
// See issue http://crbug.com/157683

// On the CookieManager methods without a callback and methods with a callback
// when that callback is null can be called from any thread, including threads
// without a message loop. Methods with a non-null callback must be called on
// a thread with a running message loop.

namespace android_webview {

// Holds a Java BooleanCookieCallback, knows how to invoke it and turn it
// into a base callback.
class BoolCookieCallbackHolder {
 public:
  BoolCookieCallbackHolder(JNIEnv* env, jobject callback) {
    callback_.Reset(env, callback);
    DCHECK(callback_);
  }

  void Invoke(bool result) {
    base::android::RunBooleanCallbackAndroid(callback_, result);
  }

  static base::RepeatingCallback<void(bool)> ConvertToCallback(
      std::unique_ptr<BoolCookieCallbackHolder> me) {
    return base::BindRepeating(&BoolCookieCallbackHolder::Invoke,
                               base::Owned(me.release()));
  }

 private:
  ScopedJavaGlobalRef<jobject> callback_;
  DISALLOW_COPY_AND_ASSIGN(BoolCookieCallbackHolder);
};

namespace {

// TODO(ntfschr): see if we can turn this into OnceCallback.
// http://crbug.com/932535.
void MaybeRunCookieCallback(base::RepeatingCallback<void(bool)> callback,
                            const bool& result) {
  if (callback)
    std::move(callback).Run(result);
}

GURL MaybeFixUpSchemeForSecureCookie(const GURL& host,
                                     const std::string& value) {
  // Log message for catching strict secure cookies related bugs.
  // TODO(sgurun) temporary. Add UMA stats to monitor, and remove afterwards.
  // http://crbug.com/933981.
  if (host.is_valid() &&
      (!host.has_scheme() || host.SchemeIs(url::kHttpScheme))) {
    net::ParsedCookie parsed_cookie(value);
    if (parsed_cookie.IsValid() && parsed_cookie.IsSecure()) {
      LOG(WARNING) << "Strict Secure Cookie policy does not allow setting a "
                      "secure cookie for "
                   << host.spec();
      GURL::Replacements replace_host;
      replace_host.SetSchemeStr("https");
      return host.ReplaceComponents(replace_host);
    }
  }
  return host;
}

// Construct a closure which signals a waitable event if and when the closure
// is called the waitable event must still exist.
static base::RepeatingClosure SignalEventClosure(WaitableEvent* completion) {
  return base::BindRepeating(&WaitableEvent::Signal,
                             base::Unretained(completion));
}

static void DiscardBool(base::RepeatingClosure f, bool b) {
  f.Run();
}

static base::RepeatingCallback<void(bool)> BoolCallbackAdapter(
    base::RepeatingClosure f) {
  return base::BindRepeating(&DiscardBool, std::move(f));
}

static void DiscardInt(base::RepeatingClosure f, int i) {
  f.Run();
}

static base::RepeatingCallback<void(int)> IntCallbackAdapter(
    base::RepeatingClosure f) {
  return base::BindRepeating(&DiscardInt, std::move(f));
}

// Are cookies allowed for file:// URLs by default?
const bool kDefaultFileSchemeAllowed = false;

}  // namespace

namespace {
base::LazyInstance<CookieManager>::Leaky g_lazy_instance;
}

// static
CookieManager* CookieManager::GetInstance() {
  return g_lazy_instance.Pointer();
}

CookieManager::CookieManager()
    : accept_file_scheme_cookies_(kDefaultFileSchemeAllowed),
      cookie_store_created_(false),
      cookie_store_client_thread_("CookieMonsterClient"),
      cookie_store_backend_thread_("CookieMonsterBackend"),
      setting_new_mojo_cookie_manager_(false) {
  cookie_store_client_thread_.Start();
  cookie_store_backend_thread_.Start();
  cookie_store_task_runner_ = cookie_store_client_thread_.task_runner();
}

CookieManager::~CookieManager() {}

// Executes the |task| on |cookie_store_task_runner_| and waits for it to
// complete before returning.
//
// To execute a CookieTask synchronously you must arrange for Signal to be
// called on the waitable event at some point. You can call the bool or int
// versions of ExecCookieTaskSync, these will supply the caller with a dummy
// callback which takes an int/bool, throws it away and calls Signal.
// Alternatively you can call the version which supplies a Closure in which
// case you must call Run on it when you want the unblock the calling code.
//
// Ignore a bool callback.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::RepeatingCallback<void(bool)>)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(base::BindOnce(
      std::move(task), BoolCallbackAdapter(SignalEventClosure(&completion))));

  // Waiting is necessary when implementing synchronous APIs for the WebView
  // embedder.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Ignore an int callback.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::RepeatingCallback<void(int)>)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(base::BindOnce(
      std::move(task), IntCallbackAdapter(SignalEventClosure(&completion))));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Call the supplied closure when you want to signal that the blocked code can
// continue.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::OnceClosure)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(
      base::BindOnce(std::move(task), SignalEventClosure(&completion)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Executes the |task| using |cookie_store_task_runner_|.
void CookieManager::ExecCookieTask(base::OnceClosure task) {
  base::AutoLock lock(task_queue_lock_);
  tasks_.push_back(std::move(task));
  // Unretained is safe, since android_webview::CookieManager is a singleton we
  // never destroy (we don't need PostTask to do any memory management).
  cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CookieManager::RunPendingCookieTasks,
                                base::Unretained(this)));
}

void CookieManager::RunPendingCookieTasks() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  // Don't do any cookie tasks if in the middle of setting a mojo CookieManager,
  // we'll call this method when that operation is finished.
  if (setting_new_mojo_cookie_manager_)
    return;

  // Copy tasks into temp_queue to minimize the amount of time in the critical
  // section, and to mitigate live-lock issues if these tasks append to the task
  // queue themselves.
  base::circular_deque<base::OnceClosure> temp_queue;
  {
    base::AutoLock lock(task_queue_lock_);
    temp_queue.swap(tasks_);
  }
  while (!temp_queue.empty()) {
    std::move(temp_queue.front()).Run();
    temp_queue.pop_front();
  }
}

base::SingleThreadTaskRunner* CookieManager::GetCookieStoreTaskRunner() {
  return cookie_store_task_runner_.get();
}

net::CookieStore* CookieManager::GetCookieStore() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  if (!cookie_store_) {
    content::CookieStoreConfig cookie_config(
        AwBrowserContext::GetCookieStorePath(),
        true /* restore_old_session_cookies */,
        true /* persist_session_cookies */, nullptr /* storage_policy */);
    cookie_config.client_task_runner = cookie_store_task_runner_;
    cookie_config.background_task_runner =
        cookie_store_backend_thread_.task_runner();

    {
      base::AutoLock lock(accept_file_scheme_cookies_lock_);

      // There are some unknowns about how to correctly handle file:// cookies,
      // and our implementation for this is not robust.  http://crbug.com/582985
      //
      // TODO(mmenke): This call should be removed once we can deprecate and
      // remove the Android WebView 'CookieManager::setAcceptFileSchemeCookies'
      // method. Until then, note that this is just not a great idea.
      cookie_config.cookieable_schemes.insert(
          cookie_config.cookieable_schemes.begin(),
          net::CookieMonster::kDefaultCookieableSchemes,
          net::CookieMonster::kDefaultCookieableSchemes +
              net::CookieMonster::kDefaultCookieableSchemesCount);
      if (accept_file_scheme_cookies_)
        cookie_config.cookieable_schemes.push_back(url::kFileScheme);
      cookie_store_created_ = true;
    }

    cookie_store_ = content::CreateCookieStore(cookie_config, nullptr);
  }

  return cookie_store_.get();
}

network::mojom::CookieManager* CookieManager::GetMojoCookieManager() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  return mojo_cookie_manager_.get();
}

void CookieManager::SetMojoCookieManager(
    network::mojom::CookieManagerPtrInfo cookie_manager_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExecCookieTaskSync(base::BindOnce(&CookieManager::SetMojoCookieManagerAsync,
                                    base::Unretained(this),
                                    std::move(cookie_manager_info)));
}

void CookieManager::SetMojoCookieManagerAsync(
    network::mojom::CookieManagerPtrInfo cookie_manager_info,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  setting_new_mojo_cookie_manager_ = true;
  // For simplicity, only permit this method to be called once (otherwise, we
  // must sometimes flush the mojo_cookie_manager_ instead of cookie_store_).
  DCHECK(!mojo_cookie_manager_.is_bound());
  if (!cookie_store_created_) {
    SwapMojoCookieManagerAsync(std::move(cookie_manager_info),
                               std::move(complete));
    return;
  }

  GetCookieStore()->FlushStore(base::BindOnce(
      &CookieManager::SwapMojoCookieManagerAsync, base::Unretained(this),
      std::move(cookie_manager_info), std::move(complete)));
}

void CookieManager::SwapMojoCookieManagerAsync(
    network::mojom::CookieManagerPtrInfo cookie_manager_info,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  mojo_cookie_manager_.Bind(std::move(cookie_manager_info));
  setting_new_mojo_cookie_manager_ = false;
  std::move(complete).Run();  // unblock content initialization
  RunPendingCookieTasks();
}

void CookieManager::SetShouldAcceptCookies(bool accept) {
  AwCookieAccessPolicy::GetInstance()->SetShouldAcceptCookies(accept);
}

bool CookieManager::GetShouldAcceptCookies() {
  return AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
}

void CookieManager::SetCookie(
    const GURL& host,
    const std::string& cookie_value,
    std::unique_ptr<BoolCookieCallbackHolder> callback_holder) {
  base::RepeatingCallback<void(bool)> callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::BindOnce(&CookieManager::SetCookieHelper,
                                base::Unretained(this), host, cookie_value,
                                callback));
}

void CookieManager::SetCookieSync(const GURL& host,
                                  const std::string& cookie_value) {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::SetCookieHelper,
                                    base::Unretained(this), host,
                                    cookie_value));
}

void CookieManager::SetCookieHelper(
    const GURL& host,
    const std::string& value,
    const base::RepeatingCallback<void(bool)> callback) {
  net::CookieOptions options;
  options.set_include_httponly();

  const GURL& new_host = MaybeFixUpSchemeForSecureCookie(host, value);

  net::CanonicalCookie::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      new_host, value, base::Time::Now(), options, &status));

  if (!cc) {
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  // Note: CookieStore and network::CookieManager have different signatures: one
  // accepts a boolean callback while the other (recently) changed to accept a
  // CookieInclusionStatus callback. WebView only cares about boolean success,
  // which is why we use |AdaptCookieInclusionStatusToBool|. This is temporary
  // technical debt until we fully launch the Network Service code path.
  if (GetMojoCookieManager()) {
    // *cc.get() is safe, because network::CookieManager::SetCanonicalCookie
    // will make a copy before our smart pointer goes out of scope.
    GetMojoCookieManager()->SetCanonicalCookie(
        *cc.get(), new_host.scheme(), options,
        net::cookie_util::AdaptCookieInclusionStatusToBool(callback));
  } else {
    GetCookieStore()->SetCanonicalCookieAsync(
        std::move(cc), new_host.scheme(), options,
        net::cookie_util::AdaptCookieInclusionStatusToBool(callback));
  }
}

std::string CookieManager::GetCookie(const GURL& host) {
  net::CookieList cookie_list;
  ExecCookieTaskSync(base::BindOnce(&CookieManager::GetCookieListAsyncHelper,
                                    base::Unretained(this), host,
                                    &cookie_list));
  return net::CanonicalCookie::BuildCookieLine(cookie_list);
}

void CookieManager::GetCookieListAsyncHelper(const GURL& host,
                                             net::CookieList* result,
                                             base::OnceClosure complete) {
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->GetCookieList(
        host, options,
        base::BindOnce(&CookieManager::GetCookieListCompleted,
                       base::Unretained(this), std::move(complete), result));
  } else {
    GetCookieStore()->GetCookieListWithOptionsAsync(
        host, options,
        base::BindOnce(&CookieManager::GetCookieListCompleted,
                       base::Unretained(this), std::move(complete), result));
  }
}

void CookieManager::GetCookieListCompleted(
    base::OnceClosure complete,
    net::CookieList* result,
    const net::CookieList& value,
    const net::CookieStatusList& excluded_cookies) {
  *result = value;
  std::move(complete).Run();
}

void CookieManager::RemoveSessionCookies(
    std::unique_ptr<BoolCookieCallbackHolder> callback_holder) {
  base::RepeatingCallback<void(bool)> callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::BindOnce(&CookieManager::RemoveSessionCookiesHelper,
                                base::Unretained(this), callback));
}

void CookieManager::RemoveSessionCookiesSync() {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::RemoveSessionCookiesHelper,
                                    base::Unretained(this)));
}

void CookieManager::RemoveSessionCookiesHelper(
    base::RepeatingCallback<void(bool)> callback) {
  if (GetMojoCookieManager()) {
    auto match_session_cookies = network::mojom::CookieDeletionFilter::New();
    match_session_cookies->session_control =
        network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
    GetMojoCookieManager()->DeleteCookies(
        std::move(match_session_cookies),
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), callback));
  } else {
    GetCookieStore()->DeleteSessionCookiesAsync(
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), callback));
  }
}

void CookieManager::RemoveCookiesCompleted(
    base::RepeatingCallback<void(bool)> callback,
    uint32_t num_deleted) {
  callback.Run(num_deleted > 0u);
}

void CookieManager::RemoveAllCookies(
    std::unique_ptr<BoolCookieCallbackHolder> callback_holder) {
  base::RepeatingCallback<void(bool)> callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::BindOnce(&CookieManager::RemoveAllCookiesHelper,
                                base::Unretained(this), callback));
}

void CookieManager::RemoveAllCookiesSync() {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::RemoveAllCookiesHelper,
                                    base::Unretained(this)));
}

void CookieManager::RemoveAllCookiesHelper(
    const base::RepeatingCallback<void(bool)> callback) {
  if (GetMojoCookieManager()) {
    // An empty filter matches all cookies.
    auto match_all_cookies = network::mojom::CookieDeletionFilter::New();
    GetMojoCookieManager()->DeleteCookies(
        std::move(match_all_cookies),
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), callback));
  } else {
    GetCookieStore()->DeleteAllAsync(
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), callback));
  }
}

void CookieManager::RemoveExpiredCookies() {
  // HasCookies will call GetAllCookiesAsync, which in turn will force a GC.
  HasCookies();
}

void CookieManager::FlushCookieStore() {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::FlushCookieStoreAsyncHelper,
                                    base::Unretained(this)));
}

void CookieManager::FlushCookieStoreAsyncHelper(base::OnceClosure complete) {
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->FlushCookieStore(std::move(complete));
  } else {
    GetCookieStore()->FlushStore(std::move(complete));
  }
}

bool CookieManager::HasCookies() {
  bool has_cookies;
  ExecCookieTaskSync(base::BindOnce(&CookieManager::HasCookiesAsyncHelper,
                                    base::Unretained(this), &has_cookies));
  return has_cookies;
}

// TODO(kristianm): Simplify this, copying the entire list around
// should not be needed.
void CookieManager::HasCookiesAsyncHelper(bool* result,
                                          base::OnceClosure complete) {
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->GetAllCookies(
        base::BindOnce(&CookieManager::HasCookiesCompleted2,
                       base::Unretained(this), std::move(complete), result));
  } else {
    GetCookieStore()->GetAllCookiesAsync(
        base::BindOnce(&CookieManager::HasCookiesCompleted,
                       base::Unretained(this), std::move(complete), result));
  }
}

void CookieManager::HasCookiesCompleted(
    base::OnceClosure complete,
    bool* result,
    const CookieList& cookies,
    const net::CookieStatusList& excluded_cookies) {
  *result = cookies.size() != 0;
  std::move(complete).Run();
}

void CookieManager::HasCookiesCompleted2(base::OnceClosure complete,
                                         bool* result,
                                         const CookieList& cookies) {
  *result = cookies.size() != 0;
  std::move(complete).Run();
}

bool CookieManager::AllowFileSchemeCookies() {
  base::AutoLock lock(accept_file_scheme_cookies_lock_);
  return accept_file_scheme_cookies_;
}

void CookieManager::SetAcceptFileSchemeCookies(bool accept) {
  base::AutoLock lock(accept_file_scheme_cookies_lock_);
  bool success;
  ExecCookieTaskSync(
      base::BindOnce(&CookieManager::AllowFileSchemeCookiesAsyncHelper,
                     base::Unretained(this), accept, &success));
  // Should only update |accept_file_scheme_cookies_| if
  // AllowFileSchemeCookiesAsyncHelper says this is OK.
  if (!success)
    return;
  accept_file_scheme_cookies_ = accept;
}

void CookieManager::AllowFileSchemeCookiesAsyncHelper(
    bool accept,
    bool* result,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->AllowFileSchemeCookies(
        accept,
        base::BindOnce(&CookieManager::AllowFileSchemeCookiesCompleted,
                       base::Unretained(this), std::move(complete), result));
  } else {
    // If we have neither a Network Service CookieManager nor have created the
    // CookieStore, we may modify |accept_file_scheme_cookies_|.
    bool can_change_cookieable_schemes = !cookie_store_created_;
    *result = can_change_cookieable_schemes;
    std::move(complete).Run();
  }
}

void CookieManager::AllowFileSchemeCookiesCompleted(base::OnceClosure complete,
                                                    bool* result,
                                                    bool value) {
  *result = value;
  std::move(complete).Run();
}

static void JNI_AwCookieManager_SetShouldAcceptCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean accept) {
  CookieManager::GetInstance()->SetShouldAcceptCookies(accept);
}

static jboolean JNI_AwCookieManager_GetShouldAcceptCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->GetShouldAcceptCookies();
}

static void JNI_AwCookieManager_SetCookie(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& value,
    const JavaParamRef<jobject>& java_callback) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));
  std::unique_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->SetCookie(host, cookie_value,
                                          std::move(callback));
}

static void JNI_AwCookieManager_SetCookieSync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& value) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));

  CookieManager::GetInstance()->SetCookieSync(host, cookie_value);
}

static ScopedJavaLocalRef<jstring> JNI_AwCookieManager_GetCookie(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url) {
  GURL host(ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(
      env, CookieManager::GetInstance()->GetCookie(host));
}

static void JNI_AwCookieManager_RemoveSessionCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_callback) {
  std::unique_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->RemoveSessionCookies(std::move(callback));
}

static void JNI_AwCookieManager_RemoveSessionCookiesSync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->RemoveSessionCookiesSync();
}

static void JNI_AwCookieManager_RemoveAllCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_callback) {
  std::unique_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->RemoveAllCookies(std::move(callback));
}

static void JNI_AwCookieManager_RemoveAllCookiesSync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->RemoveAllCookiesSync();
}

static void JNI_AwCookieManager_RemoveExpiredCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->RemoveExpiredCookies();
}

static void JNI_AwCookieManager_FlushCookieStore(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->FlushCookieStore();
}

static jboolean JNI_AwCookieManager_HasCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->HasCookies();
}

static jboolean JNI_AwCookieManager_AllowFileSchemeCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->AllowFileSchemeCookies();
}

static void JNI_AwCookieManager_SetAcceptFileSchemeCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean accept) {
  return CookieManager::GetInstance()->SetAcceptFileSchemeCookies(accept);
}

// The following two methods are used to avoid a circular project dependency.
// TODO(mmenke):  This is weird. Maybe there should be a leaky Singleton in
// browser/net that creates and owns there?

scoped_refptr<base::SingleThreadTaskRunner> GetCookieStoreTaskRunner() {
  return CookieManager::GetInstance()->GetCookieStoreTaskRunner();
}

net::CookieStore* GetCookieStore() {
  return CookieManager::GetInstance()->GetCookieStore();
}

}  // namespace android_webview
