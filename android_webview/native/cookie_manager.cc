// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/cookie_manager.h"

#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "jni/AwCookieManager_jni.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

using base::FilePath;
using base::WaitableEvent;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;
using net::CookieList;
using net::CookieMonster;

// In the future, we may instead want to inject an explicit CookieStore
// dependency into this object during process initialization to avoid
// depending on the URLRequestContext.
// See issue http://crbug.com/157683

// On the CookieManager methods without a callback and methods with a callback
// when that callback is null can be called from any thread, including threads
// without a message loop. Methods with a non-null callback must be called on
// a thread with a running message loop.

namespace android_webview {

namespace {

typedef base::Callback<void(bool)> BoolCallback;
typedef base::Callback<void(int)> IntCallback;

// Holds a Java BooleanCookieCallback, knows how to invoke it and turn it
// into a base callback.
class BoolCookieCallbackHolder {
 public:
  BoolCookieCallbackHolder(JNIEnv* env, jobject callback) {
    callback_.Reset(env, callback);
  }

  void Invoke(bool result) {
    if (!callback_.is_null()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_AwCookieManager_invokeBooleanCookieCallback(
          env, callback_.obj(), result);
    }
  }

  static BoolCallback ConvertToCallback(
      scoped_ptr<BoolCookieCallbackHolder> me) {
    return base::Bind(&BoolCookieCallbackHolder::Invoke,
                      base::Owned(me.release()));
  }

 private:
  ScopedJavaGlobalRef<jobject> callback_;
  DISALLOW_COPY_AND_ASSIGN(BoolCookieCallbackHolder);
};

// Construct a closure which signals a waitable event if and when the closure
// is called the waitable event must still exist.
static base::Closure SignalEventClosure(WaitableEvent* completion) {
  return base::Bind(&WaitableEvent::Signal, base::Unretained(completion));
}

static void DiscardBool(const base::Closure& f, bool b) {
  f.Run();
}

static BoolCallback BoolCallbackAdapter(const base::Closure& f) {
  return base::Bind(&DiscardBool, f);
}

static void DiscardInt(const base::Closure& f, int i) {
  f.Run();
}

static IntCallback IntCallbackAdapter(const base::Closure& f) {
  return base::Bind(&DiscardInt, f);
}

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

  scoped_refptr<net::CookieStore> GetCookieStore();

  void SetShouldAcceptCookies(bool accept);
  bool GetShouldAcceptCookies();
  void SetCookie(const GURL& host,
                 const std::string& cookie_value,
                 scoped_ptr<BoolCookieCallbackHolder> callback);
  void SetCookieSync(const GURL& host,
                 const std::string& cookie_value);
  std::string GetCookie(const GURL& host);
  void RemoveSessionCookies(scoped_ptr<BoolCookieCallbackHolder> callback);
  void RemoveAllCookies(scoped_ptr<BoolCookieCallbackHolder> callback);
  void RemoveAllCookiesSync();
  void RemoveSessionCookiesSync();
  void RemoveExpiredCookies();
  void FlushCookieStore();
  bool HasCookies();
  bool AllowFileSchemeCookies();
  void SetAcceptFileSchemeCookies(bool accept);

 private:
  friend struct base::DefaultLazyInstanceTraits<CookieManager>;

  CookieManager();
  ~CookieManager();

  void ExecCookieTaskSync(const base::Callback<void(BoolCallback)>& task);
  void ExecCookieTaskSync(const base::Callback<void(IntCallback)>& task);
  void ExecCookieTaskSync(const base::Callback<void(base::Closure)>& task);
  void ExecCookieTask(const base::Closure& task);

  void SetCookieHelper(
      const GURL& host,
      const std::string& value,
      BoolCallback callback);

  void GetCookieValueAsyncHelper(const GURL& host,
                                 std::string* result,
                                 base::Closure complete);
  void GetCookieValueCompleted(base::Closure complete,
                               std::string* result,
                               const std::string& value);

  void RemoveSessionCookiesHelper(BoolCallback callback);
  void RemoveAllCookiesHelper(BoolCallback callback);
  void RemoveCookiesCompleted(BoolCallback callback, int num_deleted);

  void FlushCookieStoreAsyncHelper(base::Closure complete);

  void HasCookiesAsyncHelper(bool* result, base::Closure complete);
  void HasCookiesCompleted(base::Closure complete,
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
  scoped_refptr<base::SingleThreadTaskRunner> cookie_monster_task_runner_;
  base::Lock cookie_monster_lock_;

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

  content::CookieStoreConfig cookie_config(
      cookie_store_path,
      content::CookieStoreConfig::RESTORED_SESSION_COOKIES,
      NULL, NULL);
  cookie_config.client_task_runner = client_task_runner;
  cookie_config.background_task_runner = background_task_runner;
  net::CookieStore* cookie_store = content::CreateCookieStore(cookie_config);
  cookie_monster_ = cookie_store->GetCookieMonster();
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
  cookie_monster_task_runner_ = cookie_monster_client_thread_->task_runner();
  cookie_monster_backend_thread_.reset(
      new base::Thread("CookieMonsterBackend"));
  cookie_monster_backend_thread_->Start();

  CreateCookieMonster(user_data_dir, cookie_monster_task_runner_,
                      cookie_monster_backend_thread_->task_runner());
}

// Executes the |task| on the |cookie_monster_proxy_| message loop and
// waits for it to complete before returning.

// To execute a CookieTask synchronously you must arrange for Signal to be
// called on the waitable event at some point. You can call the bool or int
// versions of ExecCookieTaskSync, these will supply the caller with a dummy
// callback which takes an int/bool, throws it away and calls Signal.
// Alternatively you can call the version which supplies a Closure in which
// case you must call Run on it when you want the unblock the calling code.

// Ignore a bool callback.
void CookieManager::ExecCookieTaskSync(
    const base::Callback<void(BoolCallback)>& task) {
  WaitableEvent completion(false, false);
  ExecCookieTask(
      base::Bind(task, BoolCallbackAdapter(SignalEventClosure(&completion))));
  ScopedAllowWaitForLegacyWebViewApi wait;
  completion.Wait();
}

// Ignore an int callback.
void CookieManager::ExecCookieTaskSync(
    const base::Callback<void(IntCallback)>& task) {
  WaitableEvent completion(false, false);
  ExecCookieTask(
      base::Bind(task, IntCallbackAdapter(SignalEventClosure(&completion))));
  ScopedAllowWaitForLegacyWebViewApi wait;
  completion.Wait();
}

// Call the supplied closure when you want to signal that the blocked code can
// continue.
void CookieManager::ExecCookieTaskSync(
    const base::Callback<void(base::Closure)>& task) {
  WaitableEvent completion(false, false);
  ExecCookieTask(base::Bind(task, SignalEventClosure(&completion)));
  ScopedAllowWaitForLegacyWebViewApi wait;
  completion.Wait();
}

// Executes the |task| on the |cookie_monster_proxy_| message loop.
void CookieManager::ExecCookieTask(const base::Closure& task) {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  cookie_monster_task_runner_->PostTask(FROM_HERE, task);
}

scoped_refptr<net::CookieStore> CookieManager::GetCookieStore() {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  return cookie_monster_;
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
    scoped_ptr<BoolCookieCallbackHolder> callback_holder) {
  BoolCallback callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::Bind(&CookieManager::SetCookieHelper,
                            base::Unretained(this),
                            host,
                            cookie_value,
                            callback));
}

void CookieManager::SetCookieSync(const GURL& host,
                              const std::string& cookie_value) {
  ExecCookieTaskSync(base::Bind(&CookieManager::SetCookieHelper,
                            base::Unretained(this),
                            host,
                            cookie_value));
}

void CookieManager::SetCookieHelper(
    const GURL& host,
    const std::string& value,
    const BoolCallback callback) {
  net::CookieOptions options;
  options.set_include_httponly();

  cookie_monster_->SetCookieWithOptionsAsync(
      host, value, options, callback);
}

std::string CookieManager::GetCookie(const GURL& host) {
  std::string cookie_value;
  ExecCookieTaskSync(base::Bind(&CookieManager::GetCookieValueAsyncHelper,
                            base::Unretained(this),
                            host,
                            &cookie_value));
  return cookie_value;
}

void CookieManager::GetCookieValueAsyncHelper(
    const GURL& host,
    std::string* result,
    base::Closure complete) {
  net::CookieOptions options;
  options.set_include_httponly();

  cookie_monster_->GetCookiesWithOptionsAsync(
      host,
      options,
      base::Bind(&CookieManager::GetCookieValueCompleted,
                 base::Unretained(this),
                 complete,
                 result));
}

void CookieManager::GetCookieValueCompleted(base::Closure complete,
                                            std::string* result,
                                            const std::string& value) {
  *result = value;
  complete.Run();
}

void CookieManager::RemoveSessionCookies(
    scoped_ptr<BoolCookieCallbackHolder> callback_holder) {
  BoolCallback callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::Bind(&CookieManager::RemoveSessionCookiesHelper,
                            base::Unretained(this),
                            callback));
}

void CookieManager::RemoveSessionCookiesSync() {
  ExecCookieTaskSync(base::Bind(&CookieManager::RemoveSessionCookiesHelper,
                            base::Unretained(this)));
}

void CookieManager::RemoveSessionCookiesHelper(
    BoolCallback callback) {
  cookie_monster_->DeleteSessionCookiesAsync(
      base::Bind(&CookieManager::RemoveCookiesCompleted,
                 base::Unretained(this),
                 callback));
}

void CookieManager::RemoveCookiesCompleted(
    BoolCallback callback,
    int num_deleted) {
  callback.Run(num_deleted > 0);
}

void CookieManager::RemoveAllCookies(
    scoped_ptr<BoolCookieCallbackHolder> callback_holder) {
  BoolCallback callback =
      BoolCookieCallbackHolder::ConvertToCallback(std::move(callback_holder));
  ExecCookieTask(base::Bind(&CookieManager::RemoveAllCookiesHelper,
                            base::Unretained(this),
                            callback));
}

void CookieManager::RemoveAllCookiesSync() {
  ExecCookieTaskSync(base::Bind(&CookieManager::RemoveAllCookiesHelper,
                            base::Unretained(this)));
}

void CookieManager::RemoveAllCookiesHelper(
    const BoolCallback callback) {
  cookie_monster_->DeleteAllAsync(
      base::Bind(&CookieManager::RemoveCookiesCompleted,
                 base::Unretained(this),
                 callback));
}

void CookieManager::RemoveExpiredCookies() {
  // HasCookies will call GetAllCookiesAsync, which in turn will force a GC.
  HasCookies();
}

void CookieManager::FlushCookieStore() {
  ExecCookieTaskSync(base::Bind(&CookieManager::FlushCookieStoreAsyncHelper,
                            base::Unretained(this)));
}

void CookieManager::FlushCookieStoreAsyncHelper(
    base::Closure complete) {
  cookie_monster_->FlushStore(complete);
}

bool CookieManager::HasCookies() {
  bool has_cookies;
  ExecCookieTaskSync(base::Bind(&CookieManager::HasCookiesAsyncHelper,
                            base::Unretained(this),
                            &has_cookies));
  return has_cookies;
}

// TODO(kristianm): Simplify this, copying the entire list around
// should not be needed.
void CookieManager::HasCookiesAsyncHelper(bool* result,
                                          base::Closure complete) {
  cookie_monster_->GetAllCookiesAsync(
      base::Bind(&CookieManager::HasCookiesCompleted,
                 base::Unretained(this),
                 complete,
                 result));
}

void CookieManager::HasCookiesCompleted(base::Closure complete,
                                        bool* result,
                                        const CookieList& cookies) {
  *result = cookies.size() != 0;
  complete.Run();
}

bool CookieManager::AllowFileSchemeCookies() {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  return AllowFileSchemeCookiesLocked();
}

bool CookieManager::AllowFileSchemeCookiesLocked() {
  return cookie_monster_->IsCookieableScheme(url::kFileScheme);
}

void CookieManager::SetAcceptFileSchemeCookies(bool accept) {
  base::AutoLock lock(cookie_monster_lock_);
  EnsureCookieMonsterExistsLocked();
  SetAcceptFileSchemeCookiesLocked(accept);
}

void CookieManager::SetAcceptFileSchemeCookiesLocked(bool accept) {
  // There are some unknowns about how to correctly handle file:// cookies,
  // and our implementation for this is not robust.  http://crbug.com/582985
  //
  // TODO(mmenke): This call should be removed once we can deprecate and remove
  // the Android WebView 'CookieManager::setAcceptFileSchemeCookies' method.
  // Until then, note that this is just not a great idea.
  std::vector<std::string> schemes;
  schemes.insert(schemes.begin(),
                 net::CookieMonster::kDefaultCookieableSchemes,
                 net::CookieMonster::kDefaultCookieableSchemes +
                     net::CookieMonster::kDefaultCookieableSchemesCount);

  if (accept)
    schemes.push_back(url::kFileScheme);

  cookie_monster_->SetCookieableSchemes(schemes);
}

}  // namespace

static void SetShouldAcceptCookies(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean accept) {
  CookieManager::GetInstance()->SetShouldAcceptCookies(accept);
}

static jboolean GetShouldAcceptCookies(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->GetShouldAcceptCookies();
}

static void SetCookie(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      const JavaParamRef<jstring>& url,
                      const JavaParamRef<jstring>& value,
                      const JavaParamRef<jobject>& java_callback) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));
  scoped_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->SetCookie(host, cookie_value,
                                          std::move(callback));
}

static void SetCookieSync(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          const JavaParamRef<jstring>& url,
                          const JavaParamRef<jstring>& value) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));

  CookieManager::GetInstance()->SetCookieSync(host, cookie_value);
}

static ScopedJavaLocalRef<jstring> GetCookie(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             const JavaParamRef<jstring>& url) {
  GURL host(ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(
      env, CookieManager::GetInstance()->GetCookie(host));
}

static void RemoveSessionCookies(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& java_callback) {
  scoped_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->RemoveSessionCookies(std::move(callback));
}

static void RemoveSessionCookiesSync(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
    CookieManager::GetInstance()->RemoveSessionCookiesSync();
}

static void RemoveAllCookies(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             const JavaParamRef<jobject>& java_callback) {
  scoped_ptr<BoolCookieCallbackHolder> callback(
      new BoolCookieCallbackHolder(env, java_callback));
  CookieManager::GetInstance()->RemoveAllCookies(std::move(callback));
}

static void RemoveAllCookiesSync(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->RemoveAllCookiesSync();
}

static void RemoveExpiredCookies(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->RemoveExpiredCookies();
}

static void FlushCookieStore(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  CookieManager::GetInstance()->FlushCookieStore();
}

static jboolean HasCookies(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->HasCookies();
}

static jboolean AllowFileSchemeCookies(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return CookieManager::GetInstance()->AllowFileSchemeCookies();
}

static void SetAcceptFileSchemeCookies(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean accept) {
  return CookieManager::GetInstance()->SetAcceptFileSchemeCookies(accept);
}

scoped_refptr<net::CookieStore> CreateCookieStore(
    AwBrowserContext* browser_context) {
  return CookieManager::GetInstance()->GetCookieStore();
}

bool RegisterCookieManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // android_webview namespace
