// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/cookie_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_crypto_delegate.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/url_constants.h"
#include "jni/AwCookieManager_jni.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/url_request/url_request_context.h"

using base::FilePath;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using content::BrowserThread;
using net::CookieList;
using net::CookieMonster;

// In the future, we may instead want to inject an explicit CookieStore
// dependency into this object during process initialization to avoid
// depending on the URLRequestContext.
// See issue http://crbug.com/157683

// All functions on the CookieManager can be called from any thread, including
// threads without a message loop. BrowserThread::IO is used to call methods
// on CookieMonster that needs to be called, and called back, on a chrome
// thread.

namespace android_webview {

namespace {

// Are cookies allowed for file:// URLs by default?
const bool kDefaultFileSchemeAllowed = false;

void ImportLegacyCookieStore(const FilePath& cookie_store_path) {
  // We use the old cookie store to create the new cookie store only if the
  // new cookie store does not exist.
  if (base::PathExists(cookie_store_path))
    return;

  // WebViewClassic gets the database path from Context and appends a
  // hardcoded name. See:
  // https://android.googlesource.com/platform/frameworks/base/+/bf6f6f9d/core/java/android/webkit/JniUtil.java
  // https://android.googlesource.com/platform/external/webkit/+/7151e/
  //     Source/WebKit/android/WebCoreSupport/WebCookieJar.cpp
  FilePath old_cookie_store_path;
  base::android::GetDatabaseDirectory(&old_cookie_store_path);
  old_cookie_store_path = old_cookie_store_path.Append(
      FILE_PATH_LITERAL("webviewCookiesChromium.db"));
  if (base::PathExists(old_cookie_store_path) &&
      !base::Move(old_cookie_store_path, cookie_store_path)) {
    LOG(WARNING) << "Failed to move old cookie store path from "
                 << old_cookie_store_path.AsUTF8Unsafe() << " to "
                 << cookie_store_path.AsUTF8Unsafe();
  }
}

void GetUserDataDir(FilePath* user_data_dir) {
  if (!PathService::Get(base::DIR_ANDROID_APP_DATA, user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
}

class CookieManager {
 public:
  static CookieManager* GetInstance();

  scoped_refptr<net::CookieStore> CreateBrowserThreadCookieStore(
      AwBrowserContext* browser_context);

  void SetAcceptCookie(bool accept);
  bool AcceptCookie();
  void SetCookie(const GURL& host, const std::string& cookie_value);
  std::string GetCookie(const GURL& host);
  void RemoveSessionCookie();
  void RemoveAllCookie();
  void RemoveExpiredCookie();
  void FlushCookieStore();
  bool HasCookies();
  bool AllowFileSchemeCookies();
  void SetAcceptFileSchemeCookies(bool accept);

 private:
  friend struct base::DefaultLazyInstanceTraits<CookieManager>;

  CookieManager();
  ~CookieManager();

  typedef base::Callback<void(base::WaitableEvent*)> CookieTask;
  void ExecCookieTask(const CookieTask& task);

  void SetCookieAsyncHelper(
      const GURL& host,
      const std::string& value,
      base::WaitableEvent* completion);
  void SetCookieCompleted(base::WaitableEvent* completion, bool success);

  void GetCookieValueAsyncHelper(
      const GURL& host,
      std::string* result,
      base::WaitableEvent* completion);
  void GetCookieValueCompleted(base::WaitableEvent* completion,
                               std::string* result,
                               const std::string& value);

  void RemoveSessionCookieAsyncHelper(base::WaitableEvent* completion);
  void RemoveAllCookieAsyncHelper(base::WaitableEvent* completion);
  void RemoveCookiesCompleted(base::WaitableEvent* completion, int num_deleted);

  void FlushCookieStoreAsyncHelper(base::WaitableEvent* completion);

  void HasCookiesAsyncHelper(bool* result,
                             base::WaitableEvent* completion);
  void HasCookiesCompleted(base::WaitableEvent* completion,
                           bool* result,
                           const CookieList& cookies);

  void CreateCookieMonster(
    const FilePath& user_data_dir,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  void EnsureCookieMonsterExistsLocked();
  bool AllowFileSchemeCookiesLocked();
  void SetAcceptFileSchemeCookiesLocked(bool accept);

  scoped_refptr<net::CookieMonster> cookie_monster_;
  scoped_refptr<base::MessageLoopProxy> cookie_monster_proxy_;
  base::Lock cookie_monster_lock_;

  // Both these threads are normally NULL. They only exist if CookieManager was
  // accessed before Chromium was started.
  scoped_ptr<base::Thread> cookie_monster_client_thread_;
  scoped_ptr<base::Thread> cookie_monster_backend_thread_;

  DISALLOW_COPY_AND_ASSIGN(CookieManager);
};

base::LazyInstance<CookieManager>::Leaky g_lazy_instance;

// static
CookieManager* CookieManager::GetInstance() {
  return g_lazy_instance.Pointer();
}

CookieManager::CookieManager() {
}

CookieManager::~CookieManager() {
}

void CookieManager::CreateCookieMonster(
    const FilePath& user_data_dir,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  FilePath cookie_store_path =
      user_data_dir.Append(FILE_PATH_LITERAL("Cookies"));

  background_task_runner->PostTask(
      FROM_HERE,
      base::Bind(ImportLegacyCookieStore, cookie_store_path));

  net::CookieStore* cookie_store = content::CreatePersistentCookieStore(
    cookie_store_path,
    true,
    NULL,
    NULL,
    client_task_runner,
    background_task_runner,
    scoped_ptr<content::CookieCryptoDelegate>());
  cookie_monster_ = cookie_store->GetCookieMonster();
  cookie_monster_->SetPersistSessionCookies(true);
  SetAcceptFileSchemeCookiesLocked(kDefaultFileSchemeAllowed);
}

void CookieManager::EnsureCookieMonsterExistsLocked() {
  cookie_monster_lock_.AssertAcquired();
  if (cookie_monster_.get()) {
    return;
  }

  // Create cookie monster using WebView-specific threads, as the rest of the
  // browser has not been started yet.
  FilePath user_data_dir;
  GetUserDataDir(&user_data_dir);
  cookie_monster_client_thread_.reset(
      new base::Thread("CookieMonsterClient"));
  cookie_monster_client_thread_->Start();
  cookie_monster_proxy_ = cookie_monster_client_thread_->message_loop_proxy();
  cookie_monster_backend_thread_.reset(
      new base::Thread("CookieMonsterBackend"));
  cookie_monster_backend_thread_->Start();

  CreateCookieMonster(user_data_dir,
                      cookie_monster_proxy_,
                      cookie_monster_backend_thread_->message_loop_proxy());
}

// Executes the |task| on the |cookie_monster_proxy_| message loop.
void CookieManager::ExecCookieTask(const CookieTask& task) {
  base::WaitableEvent completion(false, false);
  base::AutoLock lock(cookie_monster_lock_);

  EnsureCookieMonsterExistsLocked();

  cookie_monster_proxy_->PostTask(FROM_HERE, base::Bind(task, &completion));

  // We always wait for the posted task to complete, even when it doesn't return
  // a value, because previous versions of the CookieManager API were
  // synchronous in most/all cases and the caller may be relying on this.
  ScopedAllowWaitForLegacyWebViewApi wait;
  completion.Wait();
}

scoped_refptr<net::CookieStore> CookieManager::CreateBrowserThreadCookieStore(
    AwBrowserContext* browser_context) {
  base::AutoLock lock(cookie_monster_lock_);

  if (cookie_monster_client_thread_) {
    // We created a cookie monster already on its own threads; we'll just keep
    // using it rather than creating one on the normal Chromium threads.
    // CookieMonster is threadsafe, so this is fine.
    return cookie_monster_;
  }

  // Go ahead and create the cookie monster using the normal Chromium threads.
  DCHECK(!cookie_monster_.get());
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::IO));

  FilePath user_data_dir;
  GetUserDataDir(&user_data_dir);
  DCHECK(browser_context->GetPath() == user_data_dir);

  cookie_monster_proxy_ =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          BrowserThread::GetBlockingPool()->GetSequenceToken());
  CreateCookieMonster(user_data_dir,
                      cookie_monster_proxy_,
                      background_task_runner);
  return cookie_monster_;
}

void CookieManager::SetAcceptCookie(bool accept) {
  AwCookieAccessPolicy::GetInstance()->SetGlobalAllowAccess(accept);
}

bool CookieManager::AcceptCookie() {
  return AwCookieAccessPolicy::GetInstance()->GetGlobalAllowAccess();
}

void CookieManager::SetCookie(const GURL& host,
                              const std::string& cookie_value) {
  ExecCookieTask(base::Bind(&CookieManager::SetCookieAsyncHelper,
                            base::Unretained(this),
                            host,
                            cookie_value));
}

void CookieManager::SetCookieAsyncHelper(
    const GURL& host,
    const std::string& value,
    base::WaitableEvent* completion) {
  net::CookieOptions options;
  options.set_include_httponly();

  cookie_monster_->SetCookieWithOptionsAsync(
      host, value, options,
      base::Bind(&CookieManager::SetCookieCompleted,
                 base::Unretained(this),
                 completion));
}

void CookieManager::SetCookieCompleted(base::WaitableEvent* completion,
                                       bool success) {
  // The CookieManager API does not return a value for SetCookie,
  // so we don't need to propagate the |success| value back to the caller.
  completion->Signal();
}

std::string CookieManager::GetCookie(const GURL& host) {
  std::string cookie_value;
  ExecCookieTask(base::Bind(&CookieManager::GetCookieValueAsyncHelper,
                            base::Unretained(this),
                            host,
                            &cookie_value));

  return cookie_value;
}

void CookieManager::GetCookieValueAsyncHelper(
    const GURL& host,
    std::string* result,
    base::WaitableEvent* completion) {
  net::CookieOptions options;
  options.set_include_httponly();

  cookie_monster_->GetCookiesWithOptionsAsync(
      host,
      options,
      base::Bind(&CookieManager::GetCookieValueCompleted,
                 base::Unretained(this),
                 completion,
                 result));
}

void CookieManager::GetCookieValueCompleted(base::WaitableEvent* completion,
                                            std::string* result,
                                            const std::string& value) {
  *result = value;
  completion->Signal();
}

void CookieManager::RemoveSessionCookie() {
  ExecCookieTask(base::Bind(&CookieManager::RemoveSessionCookieAsyncHelper,
                            base::Unretained(this)));
}

void CookieManager::RemoveSessionCookieAsyncHelper(
    base::WaitableEvent* completion) {
  cookie_monster_->DeleteSessionCookiesAsync(
      base::Bind(&CookieManager::RemoveCookiesCompleted,
                 base::Unretained(this),
                 completion));
}

void CookieManager::RemoveCookiesCompleted(base::WaitableEvent* completion,
                                           int num_deleted) {
  // The CookieManager API does not return a value for removeSessionCookie or
  // removeAllCookie, so we don't need to propagate the |num_deleted| value back
  // to the caller.
  completion->Signal();
}

void CookieManager::RemoveAllCookie() {
  ExecCookieTask(base::Bind(&CookieManager::RemoveAllCookieAsyncHelper,
                            base::Unretained(this)));
}

void CookieManager::RemoveAllCookieAsyncHelper(
    base::WaitableEvent* completion) {
  cookie_monster_->DeleteAllAsync(
      base::Bind(&CookieManager::RemoveCookiesCompleted,
                 base::Unretained(this),
                 completion));
}

void CookieManager::RemoveExpiredCookie() {
  // HasCookies will call GetAllCookiesAsync, which in turn will force a GC.
  HasCookies();
}

void CookieManager::FlushCookieStoreAsyncHelper(
    base::WaitableEvent* completion) {
  cookie_monster_->FlushStore(base::Bind(&base::WaitableEvent::Signal,
                                         base::Unretained(completion)));
}

void CookieManager::FlushCookieStore() {
  ExecCookieTask(base::Bind(&CookieManager::FlushCookieStoreAsyncHelper,
                            base::Unretained(this)));
}

bool CookieManager::HasCookies() {
  bool has_cookies;
  ExecCookieTask(base::Bind(&CookieManager::HasCookiesAsyncHelper,
                            base::Unretained(this),
                            &has_cookies));
  return has_cookies;
}

// TODO(kristianm): Simplify this, copying the entire list around
// should not be needed.
void CookieManager::HasCookiesAsyncHelper(bool* result,
                                  base::WaitableEvent* completion) {
  cookie_monster_->GetAllCookiesAsync(
      base::Bind(&CookieManager::HasCookiesCompleted,
                 base::Unretained(this),
                 completion,
                 result));
}

void CookieManager::HasCookiesCompleted(base::WaitableEvent* completion,
                                        bool* result,
                                        const CookieList& cookies) {
  *result = cookies.size() != 0;
  completion->Signal();
}

bool CookieManager::AllowFileSchemeCookies() {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  return AllowFileSchemeCookiesLocked();
}

bool CookieManager::AllowFileSchemeCookiesLocked() {
  return cookie_monster_->IsCookieableScheme(chrome::kFileScheme);
}

void CookieManager::SetAcceptFileSchemeCookies(bool accept) {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  SetAcceptFileSchemeCookiesLocked(accept);
}

void CookieManager::SetAcceptFileSchemeCookiesLocked(bool accept) {
  // The docs on CookieManager base class state the API must not be called after
  // creating a CookieManager instance (which contradicts its own internal
  // implementation) but this code does rely on the essence of that comment, as
  // the monster will DCHECK here if it has already been lazy initialized (i.e.
  // if cookies have been read or written from the store). If that turns out to
  // be a problemin future, it looks like it maybe possible to relax the DCHECK.
  cookie_monster_->SetEnableFileScheme(accept);
}

}  // namespace

static void SetAcceptCookie(JNIEnv* env, jobject obj, jboolean accept) {
  CookieManager::GetInstance()->SetAcceptCookie(accept);
}

static jboolean AcceptCookie(JNIEnv* env, jobject obj) {
  return CookieManager::GetInstance()->AcceptCookie();
}

static void SetCookie(JNIEnv* env, jobject obj, jstring url, jstring value) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));

  CookieManager::GetInstance()->SetCookie(host, cookie_value);
}

static jstring GetCookie(JNIEnv* env, jobject obj, jstring url) {
  GURL host(ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(
      env,
      CookieManager::GetInstance()->GetCookie(host)).Release();
}

static void RemoveSessionCookie(JNIEnv* env, jobject obj) {
  CookieManager::GetInstance()->RemoveSessionCookie();
}

static void RemoveAllCookie(JNIEnv* env, jobject obj) {
  CookieManager::GetInstance()->RemoveAllCookie();
}

static void RemoveExpiredCookie(JNIEnv* env, jobject obj) {
  CookieManager::GetInstance()->RemoveExpiredCookie();
}

static void FlushCookieStore(JNIEnv* env, jobject obj) {
  CookieManager::GetInstance()->FlushCookieStore();
}

static jboolean HasCookies(JNIEnv* env, jobject obj) {
  return CookieManager::GetInstance()->HasCookies();
}

static jboolean AllowFileSchemeCookies(JNIEnv* env, jobject obj) {
  return CookieManager::GetInstance()->AllowFileSchemeCookies();
}

static void SetAcceptFileSchemeCookies(JNIEnv* env, jobject obj,
                                       jboolean accept) {
  return CookieManager::GetInstance()->SetAcceptFileSchemeCookies(accept);
}

scoped_refptr<net::CookieStore> CreateCookieStore(
    AwBrowserContext* browser_context) {
  return CookieManager::GetInstance()->CreateBrowserThreadCookieStore(
      browser_context);
}

bool RegisterCookieManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // android_webview namespace
