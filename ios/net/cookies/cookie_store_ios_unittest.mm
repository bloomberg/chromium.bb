// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_store_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Clears the underlying NSHTTPCookieStorage.
void ClearCookies() {
  NSHTTPCookieStorage* store = [NSHTTPCookieStorage sharedHTTPCookieStorage];
  [store setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
  NSArray* cookies = [store cookies];
  for (NSHTTPCookie* cookie in cookies)
    [store deleteCookie:cookie];
  EXPECT_EQ(0u, [[store cookies] count]);
}
}  // namespace

namespace net {

struct CookieStoreIOSTestTraits {
  static net::CookieStore* Create() {
    ClearCookies();
    CookieStoreIOS* store = new CookieStoreIOS(nullptr);
    store->synchronization_state_ = CookieStoreIOS::SYNCHRONIZED;
    return store;
  }

  static const bool is_cookie_monster = false;
  static const bool supports_http_only = false;
  static const bool supports_non_dotted_domains = false;
  static const bool preserves_trailing_dots = false;
  static const bool filters_schemes = false;
  static const bool has_path_prefix_bug = true;
  static const int creation_time_granularity_in_ms = 1000;
  static const bool enforce_strict_secure = false;

  base::MessageLoop loop_;
};

struct InactiveCookieStoreIOSTestTraits {
  static scoped_refptr<net::CookieStore> Create() {
    return new CookieStoreIOS(nullptr);
  }

  static const bool is_cookie_monster = false;
  static const bool supports_http_only = false;
  static const bool supports_non_dotted_domains = true;
  static const bool preserves_trailing_dots = true;
  static const bool filters_schemes = false;
  static const bool has_path_prefix_bug = false;
  static const int creation_time_granularity_in_ms = 0;
  static const int enforces_prefixes = true;
  static const bool enforce_strict_secure = false;

  base::MessageLoop loop_;
};

// RoundTripTestCookieStore is un-synchronized and re-synchronized after all
// cookie operations. This means all system cookies are converted to Chrome
// cookies and converted back.
// The purpose of this class is to test that cookie conversions do not break the
// cookie store.
class RoundTripTestCookieStore : public net::CookieStore {
 public:
  RoundTripTestCookieStore()
      : store_(new CookieStoreIOS(nullptr)),
        dummy_store_(new CookieStoreIOS(nullptr)) {
    CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  }

  // Inherited CookieStore methods.
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 const SetCookiesCallback& callback) override {
    RoundTrip();
    store_->SetCookieWithOptionsAsync(url, cookie_line, options, callback);
  }

  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  const GetCookiesCallback& callback) override {
    RoundTrip();
    store_->GetCookiesWithOptionsAsync(url, options, callback);
  }

  void GetAllCookiesForURLAsync(
      const GURL& url,
      const GetCookieListCallback& callback) override {
    RoundTrip();
    store_->GetAllCookiesForURLAsync(url, callback);
  }

  void GetAllCookiesAsync(const GetCookieListCallback& callback) override {
    RoundTrip();
    store_->GetAllCookiesAsync(callback);
  }

  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override {
    RoundTrip();
    store_->DeleteCookieAsync(url, cookie_name, callback);
  }

  net::CookieMonster* GetCookieMonster() override { return nullptr; }

  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override {
    RoundTrip();
    store_->DeleteAllCreatedBetweenAsync(delete_begin, delete_end, callback);
  }

  void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) override {
    RoundTrip();
    store_->DeleteAllCreatedBetweenForHostAsync(delete_begin, delete_end, url,
                                                callback);
  }

  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override {
    RoundTrip();
    store_->DeleteSessionCookiesAsync(callback);
  }

  void FlushStore(const base::Closure& callback) override {
    RoundTrip();
    store_->FlushStore(callback);
  }

  scoped_ptr<CookieStore::CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override {
    return scoped_ptr<CookieStore::CookieChangedSubscription>();
  }

 protected:
  ~RoundTripTestCookieStore() override { store_->UnSynchronize(); }

 private:
  void RoundTrip() {
    CookieStoreIOS::SwitchSynchronizedStore(store_.get(), dummy_store_.get());
    // Check that the system store is empty, because it is synchronized with
    // |dummy_store_| which is empty.
    NSHTTPCookieStorage* store = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    EXPECT_EQ(0u, [[store cookies] count]);
    CookieStoreIOS::SwitchSynchronizedStore(dummy_store_.get(), store_.get());
  }

  scoped_refptr<CookieStoreIOS> store_;
  // |dummy_store_| is not directly used, but is needed to make |store_|
  // inactive.
  scoped_refptr<CookieStoreIOS> dummy_store_;
};

struct RoundTripTestCookieStoreTraits {
  static scoped_refptr<net::CookieStore> Create() {
    ClearCookies();
    return new RoundTripTestCookieStore();
  }

  static const bool is_cookie_monster = false;
  static const bool supports_http_only = false;
  static const bool supports_non_dotted_domains = false;
  static const bool preserves_trailing_dots = false;
  static const bool filters_schemes = false;
  static const bool has_path_prefix_bug = true;
  static const int creation_time_granularity_in_ms = 1000;
  static const int enforces_prefixes = true;
  static const bool enforce_strict_secure = false;
};

}  // namespace net

namespace net {

INSTANTIATE_TYPED_TEST_CASE_P(CookieStoreIOS,
                              CookieStoreTest,
                              CookieStoreIOSTestTraits);

INSTANTIATE_TYPED_TEST_CASE_P(InactiveCookieStoreIOS,
                              CookieStoreTest,
                              InactiveCookieStoreIOSTestTraits);

INSTANTIATE_TYPED_TEST_CASE_P(RoundTripTestCookieStore,
                              CookieStoreTest,
                              RoundTripTestCookieStoreTraits);

}  // namespace net

namespace {

// Test net::CookieMonster::PersistentCookieStore allowing to control when the
// initialization completes.
class TestPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  TestPersistentCookieStore()
      : kTestCookieURL("http://foo.google.com/bar"), flushed_(false) {}

  // Runs the completion callback with a "a=b" cookie.
  void RunLoadedCallback() {
    std::vector<net::CanonicalCookie*> cookies;
    net::CookieOptions options;
    options.set_include_httponly();

    scoped_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        kTestCookieURL, "a=b", base::Time::Now(), options));
    cookies.push_back(cookie.release());

    // Some canonical cookies cannot be converted into System cookies, for
    // example if value is not valid utf8. Such cookies are ignored.
    net::CanonicalCookie* bad_canonical_cookie = new net::CanonicalCookie(
        kTestCookieURL, "name", "\x81r\xe4\xbd\xa0\xe5\xa5\xbd", "domain",
        "path/",
        base::Time(),  // creation
        base::Time(),  // expires
        base::Time(),  // last_access
        false,         // secure
        false,         // httponly
        false,         // same_site
        net::COOKIE_PRIORITY_DEFAULT);
    cookies.push_back(bad_canonical_cookie);
    loaded_callback_.Run(cookies);
  }

  bool flushed() { return flushed_; }

 private:
  // net::CookieMonster::PersistentCookieStore implementation:
  void Load(const LoadedCallback& loaded_callback) override {
    loaded_callback_ = loaded_callback;
  }

  void LoadCookiesForKey(const std::string& key,
                         const LoadedCallback& loaded_callback) override {
    loaded_callback_ = loaded_callback;
  }

  void AddCookie(const net::CanonicalCookie& cc) override {}
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override {}
  void DeleteCookie(const net::CanonicalCookie& cc) override {}
  void SetForceKeepSessionState() override {}
  void Flush(const base::Closure& callback) override { flushed_ = true; }

 private:
  ~TestPersistentCookieStore() override {}

  const GURL kTestCookieURL;
  LoadedCallback loaded_callback_;
  bool flushed_;
};

// Helper callback to be passed to CookieStore::GetCookiesWithOptionsAsync().
class GetCookieCallback {
 public:
  GetCookieCallback() : did_run_(false) {}

  // Returns true if the callback has been run.
  bool did_run() { return did_run_; }

  // Returns the parameter of the callback.
  const std::string& cookie_line() { return cookie_line_; }

  void Run(const std::string& cookie_line) {
    ASSERT_FALSE(did_run_);
    did_run_ = true;
    cookie_line_ = cookie_line;
  }

 private:
  bool did_run_;
  std::string cookie_line_;
};

// Helper callback to be passed to CookieStore::GetAllCookiesForURLAsync().
class GetAllCookiesCallback {
 public:
  GetAllCookiesCallback() : did_run_(false) {}

  // Returns true if the callback has been run.
  bool did_run() { return did_run_; }

  // Returns the parameter of the callback.
  const net::CookieList& cookie_list() { return cookie_list_; }

  void Run(const net::CookieList& cookie_list) {
    ASSERT_FALSE(did_run_);
    did_run_ = true;
    cookie_list_ = cookie_list;
  }

 private:
  bool did_run_;
  net::CookieList cookie_list_;
};

namespace {

void RecordCookieChanges(std::vector<net::CanonicalCookie>* out_cookies,
                         std::vector<bool>* out_removes,
                         const net::CanonicalCookie& cookie,
                         bool removed) {
  DCHECK(out_cookies);
  out_cookies->push_back(cookie);
  if (out_removes)
    out_removes->push_back(removed);
}

void IgnoreBoolean(bool ignored) {
}

void IgnoreString(const std::string& ignored) {
}

}  // namespace

class CookieStoreIOSWithBackend : public testing::Test {
 public:
  CookieStoreIOSWithBackend()
      : kTestCookieURL("http://foo.google.com/bar"),
        kTestCookieURL2("http://foo.google.com/baz"),
        kTestCookieURL3("http://foo.google.com"),
        kTestCookieURL4("http://bar.google.com/bar") {
    backend_ = new TestPersistentCookieStore;
    store_ = new net::CookieStoreIOS(backend_.get());
    net::CookieStoreIOS::SetCookiePolicy(net::CookieStoreIOS::ALLOW);
    cookie_changed_callback_ = store_->AddCallbackForCookie(
        kTestCookieURL, "abc",
        base::Bind(&RecordCookieChanges, &cookies_changed_, &cookies_removed_));
  }

  ~CookieStoreIOSWithBackend() override {}

  // Gets the cookies. |callback| will be called on completion.
  void GetCookies(const net::CookieStore::GetCookiesCallback& callback) {
    net::CookieOptions options;
    options.set_include_httponly();
    store_->GetCookiesWithOptionsAsync(kTestCookieURL, options, callback);
  }

  // Sets a cookie.
  void SetCookie(const std::string& cookie_line) {
    net::CookieOptions options;
    options.set_include_httponly();
    store_->SetCookieWithOptionsAsync(kTestCookieURL, cookie_line, options,
                                      base::Bind(&IgnoreBoolean));
    net::CookieStoreIOS::NotifySystemCookiesChanged();
    // Wait until the flush is posted.
    base::MessageLoop::current()->RunUntilIdle();
  }

  void SetSystemCookie(const GURL& url,
                       const std::string& name,
                       const std::string& value) {
    NSHTTPCookieStorage* storage =
        [NSHTTPCookieStorage sharedHTTPCookieStorage];
    [storage setCookie:[NSHTTPCookie cookieWithProperties:@{
      NSHTTPCookiePath : base::SysUTF8ToNSString(url.path()),
      NSHTTPCookieName : base::SysUTF8ToNSString(name),
      NSHTTPCookieValue : base::SysUTF8ToNSString(value),
      NSHTTPCookieDomain : base::SysUTF8ToNSString(url.host()),
    }]];
    net::CookieStoreIOS::NotifySystemCookiesChanged();
    base::MessageLoop::current()->RunUntilIdle();
  }

  void DeleteSystemCookie(const GURL& gurl, const std::string& name) {
    NSHTTPCookieStorage* storage =
        [NSHTTPCookieStorage sharedHTTPCookieStorage];
    NSURL* nsurl = net::NSURLWithGURL(gurl);
    NSArray* cookies = [storage cookiesForURL:nsurl];
    for (NSHTTPCookie* cookie in cookies) {
      if (cookie.name.UTF8String == name) {
        [storage deleteCookie:cookie];
        break;
      }
    }
    net::CookieStoreIOS::NotifySystemCookiesChanged();
    base::MessageLoop::current()->RunUntilIdle();
  }

 protected:
  const GURL kTestCookieURL;
  const GURL kTestCookieURL2;
  const GURL kTestCookieURL3;
  const GURL kTestCookieURL4;

  base::MessageLoop loop_;
  scoped_refptr<TestPersistentCookieStore> backend_;
  scoped_refptr<net::CookieStoreIOS> store_;
  scoped_ptr<net::CookieStore::CookieChangedSubscription>
      cookie_changed_callback_;
  std::vector<net::CanonicalCookie> cookies_changed_;
  std::vector<bool> cookies_removed_;
};

}  // namespace

namespace net {

TEST_F(CookieStoreIOSWithBackend, SetCookieCallsHookWhenNotSynchronized) {
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(0U, cookies_changed_.size());
  EXPECT_EQ(0U, cookies_removed_.size());
  backend_->RunLoadedCallback();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, cookies_changed_.size());
  EXPECT_EQ(1U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[0].Name());
  EXPECT_EQ("def", cookies_changed_[0].Value());
  EXPECT_FALSE(cookies_removed_[0]);

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  SetCookie("abc=ghi");
  EXPECT_EQ(3U, cookies_changed_.size());
  EXPECT_EQ(3U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[1].Name());
  EXPECT_EQ("def", cookies_changed_[1].Value());
  EXPECT_TRUE(cookies_removed_[1]);
  EXPECT_EQ("abc", cookies_changed_[2].Name());
  EXPECT_EQ("ghi", cookies_changed_[2].Value());
  EXPECT_FALSE(cookies_removed_[2]);

  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, SetCookieCallsHookWhenSynchronized) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  GetCookies(base::Bind(&IgnoreString));
  backend_->RunLoadedCallback();
  base::MessageLoop::current()->RunUntilIdle();
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  EXPECT_EQ(1U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[0].Name());
  EXPECT_EQ("def", cookies_changed_[0].Value());
  EXPECT_FALSE(cookies_removed_[0]);

  SetCookie("abc=ghi");
  EXPECT_EQ(3U, cookies_changed_.size());
  EXPECT_EQ(3U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[1].Name());
  EXPECT_EQ("def", cookies_changed_[1].Value());
  EXPECT_TRUE(cookies_removed_[1]);
  EXPECT_EQ("abc", cookies_changed_[2].Name());
  EXPECT_EQ("ghi", cookies_changed_[2].Value());
  EXPECT_FALSE(cookies_removed_[2]);
  DeleteSystemCookie(kTestCookieURL, "abc");

  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, DeleteCallsHook) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  GetCookies(base::Bind(&IgnoreString));
  backend_->RunLoadedCallback();
  base::MessageLoop::current()->RunUntilIdle();
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  EXPECT_EQ(1U, cookies_removed_.size());
  store_->DeleteCookieAsync(kTestCookieURL, "abc",
                            base::Bind(&IgnoreBoolean, false));
  CookieStoreIOS::NotifySystemCookiesChanged();
  base::MessageLoop::current()->RunUntilIdle();
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, SameValueDoesNotCallHook) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  GetCookieCallback callback;
  GetCookies(base::Bind(&IgnoreString));
  backend_->RunLoadedCallback();
  base::MessageLoop::current()->RunUntilIdle();
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  store_->UnSynchronize();
}

TEST(CookieStoreIOS, GetAllCookiesForURLAsync) {
  base::MessageLoop loop;
  const GURL kTestCookieURL("http://foo.google.com/bar");
  ClearCookies();
  scoped_refptr<CookieStoreIOS> cookie_store(new CookieStoreIOS(nullptr));
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, cookie_store.get());
  // Add a cookie.
  net::CookieOptions options;
  options.set_include_httponly();
  cookie_store->SetCookieWithOptionsAsync(
      kTestCookieURL, "a=b", options, net::CookieStore::SetCookiesCallback());
  // Disallow cookies.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  // No cookie in the system store.
  NSHTTPCookieStorage* system_store =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  EXPECT_EQ(0u, [[system_store cookies] count]);
  // Flushing should not have any effect.
  cookie_store->FlushStore(base::Closure());
  // Check we can get the cookie even though cookies are disabled.
  GetAllCookiesCallback callback;
  cookie_store->GetAllCookiesForURLAsync(
      kTestCookieURL,
      base::Bind(&GetAllCookiesCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1u, callback.cookie_list().size());
  net::CanonicalCookie cookie = callback.cookie_list()[0];
  EXPECT_EQ("a", cookie.Name());
  EXPECT_EQ("b", cookie.Value());
  // Re-enable cookies.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::ALLOW);
  // Cookie is back in the system store.
  EXPECT_EQ(1u, [[system_store cookies] count]);
  cookie_store->UnSynchronize();
}

// Tests that cookies can be read before the backend is loaded.
TEST_F(CookieStoreIOSWithBackend, NotSynchronized) {
  // Start fetching the cookie.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Backend loading completes.
  backend_->RunLoadedCallback();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
}

// Tests that cookies can be read before synchronization is complete.
TEST_F(CookieStoreIOSWithBackend, Synchronizing) {
  // Start synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Backend loading completes (end of synchronization).
  backend_->RunLoadedCallback();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  store_->UnSynchronize();
}

// Tests that cookies can be read before synchronization is complete, when
// triggered by a change in cookie policy.
TEST_F(CookieStoreIOSWithBackend, SynchronizingAfterPolicyChange) {
  ClearCookies();
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  // SwitchSynchronizedStore() does nothing when cookies are blocked.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Start synchronization by allowing cookies.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::ALLOW);
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Backend loading completes (end of synchronization).
  backend_->RunLoadedCallback();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  store_->UnSynchronize();
}

// Tests that Synchronization can be "aborted" (i.e. the cookie store is
// unsynchronized while synchronization is in progress).
TEST_F(CookieStoreIOSWithBackend, SyncThenUnsync) {
  ClearCookies();
  scoped_refptr<CookieStoreIOS> dummy_store = new CookieStoreIOS(nullptr);
  // Switch back and forth before synchronization can complete.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  CookieStoreIOS::SwitchSynchronizedStore(store_.get(), dummy_store.get());
  backend_->RunLoadedCallback();
  // No cookie leak in the system store.
  NSHTTPCookieStorage* store = [NSHTTPCookieStorage sharedHTTPCookieStorage];
  EXPECT_EQ(0u, [[store cookies] count]);
  // No cookie lost.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  dummy_store->UnSynchronize();
}

// Tests that Synchronization can be "aborted" while there are pending tasks
// (i.e. the cookie store is unsynchronized while synchronization is in progress
// and there are pending tasks).
TEST_F(CookieStoreIOSWithBackend, SyncThenUnsyncWithPendingTasks) {
  ClearCookies();
  scoped_refptr<CookieStoreIOS> dummy_store = new CookieStoreIOS(nullptr);
  // Start synchornization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Create a pending task while synchronization is in progress.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Cancel the synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(store_.get(), dummy_store.get());
  // Synchronization completes after being cancelled.
  backend_->RunLoadedCallback();
  // The task is not lost.
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  dummy_store->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, ChangePolicyOnceDuringSynchronization) {
  // Start synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Toggle cookie policy to trigger another synchronization while the first one
  // is still in progress.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  // Backend loading completes (end of synchronization).
  backend_->RunLoadedCallback();
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::ALLOW);
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend,
       ChangePolicyDuringSynchronizationWithPendingTask) {
  // Start synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Create a pending task while synchronization is in progress.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Toggle cookie policy to trigger another synchronization while the first one
  // is still in progress.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  // Backend loading completes (end of synchronization).
  backend_->RunLoadedCallback();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, ChangePolicyTwiceDuringSynchronization) {
  // Start synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Toggle cookie policy to trigger another synchronization while the first one
  // is still in progress.
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::ALLOW);
  // Backend loading completes (end of synchronization).
  backend_->RunLoadedCallback();
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, UnSynchronizeBeforeLoadComplete) {
  ClearCookies();
  // Switch back and forth before synchronization can complete.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  store_->UnSynchronize();
  backend_->RunLoadedCallback();
  // No cookie lost.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
}

TEST_F(CookieStoreIOSWithBackend, UnSynchronize) {
  ClearCookies();
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  store_->UnSynchronize();
  // No cookie lost.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
}

TEST_F(CookieStoreIOSWithBackend, FlushOnUnSynchronize) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  EXPECT_FALSE(backend_->flushed());
  store_->UnSynchronize();
  EXPECT_TRUE(backend_->flushed());
}

TEST_F(CookieStoreIOSWithBackend, FlushOnSwitch) {
  scoped_refptr<CookieStoreIOS> dummy_store = new CookieStoreIOS(nullptr);
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  EXPECT_FALSE(backend_->flushed());
  CookieStoreIOS::SwitchSynchronizedStore(store_.get(), dummy_store.get());
  EXPECT_TRUE(backend_->flushed());
  dummy_store->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, FlushOnCookieChanged) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  store_->set_flush_delay_for_testing(base::TimeDelta());
  backend_->RunLoadedCallback();
  EXPECT_FALSE(backend_->flushed());

  // Set a cookie an check that it triggers a flush.
  SetCookie("x=y");
  EXPECT_TRUE(backend_->flushed());

  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, ManualFlush) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  EXPECT_FALSE(backend_->flushed());

  // The store should be flushed even if it is not dirty.
  store_->FlushStore(base::Closure());
  EXPECT_TRUE(backend_->flushed());

  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, FlushOnPolicyChange) {
  // Start synchronization.
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  // Toggle cookie policy to trigger a flush.
  EXPECT_FALSE(backend_->flushed());
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::BLOCK);
  EXPECT_TRUE(backend_->flushed());
  store_->UnSynchronize();
  CookieStoreIOS::SetCookiePolicy(CookieStoreIOS::ALLOW);
}

TEST_F(CookieStoreIOSWithBackend, NoInitialNotifyWithNoCookie) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  std::vector<net::CanonicalCookie> cookies;
  store_->AddCallbackForCookie(
      kTestCookieURL, "abc",
      base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, NoInitialNotifyWithSystemCookie) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  std::vector<net::CanonicalCookie> cookies;
  store_->AddCallbackForCookie(
      kTestCookieURL, "abc",
      base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, NotifyOnAdd) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  EXPECT_EQ(0U, removes.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());
  EXPECT_EQ("abc", cookies[0].Name());
  EXPECT_EQ("def", cookies[0].Value());
  EXPECT_FALSE(removes[0]);

  SetSystemCookie(kTestCookieURL, "ghi", "jkl");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());

  DeleteSystemCookie(kTestCookieURL, "abc");
  DeleteSystemCookie(kTestCookieURL, "ghi");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, NotifyOnChange) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "ghi");
  EXPECT_EQ(3U, cookies.size());
  EXPECT_EQ(3U, removes.size());
  EXPECT_EQ("abc", cookies[1].Name());
  EXPECT_EQ("def", cookies[1].Value());
  EXPECT_TRUE(removes[1]);
  EXPECT_EQ("abc", cookies[2].Name());
  EXPECT_EQ("ghi", cookies[2].Value());
  EXPECT_FALSE(removes[2]);

  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, NotifyOnDelete) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  SetSystemCookie(kTestCookieURL, "abc", "def");
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());
  EXPECT_TRUE(removes[0]);
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(2U, cookies.size());
  EXPECT_EQ(2U, removes.size());
  EXPECT_FALSE(removes[1]);
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, NoNotifyOnNoChange) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, MultipleNotifies) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> cookies2;
  std::vector<net::CanonicalCookie> cookies3;
  std::vector<net::CanonicalCookie> cookies4;
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle2 =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies2, nullptr));
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle3 =
      store_->AddCallbackForCookie(
          kTestCookieURL3, "abc",
          base::Bind(&RecordCookieChanges, &cookies3, nullptr));
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle4 =
      store_->AddCallbackForCookie(
          kTestCookieURL4, "abc",
          base::Bind(&RecordCookieChanges, &cookies4, nullptr));
  SetSystemCookie(kTestCookieURL, "abc", "def");
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  SetSystemCookie(kTestCookieURL4, "abc", "def");
  EXPECT_EQ(2U, cookies.size());
  EXPECT_EQ(2U, cookies2.size());
  EXPECT_EQ(1U, cookies3.size());
  EXPECT_EQ(1U, cookies4.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  DeleteSystemCookie(kTestCookieURL2, "abc");
  DeleteSystemCookie(kTestCookieURL3, "abc");
  DeleteSystemCookie(kTestCookieURL4, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, LessSpecificNestedCookie) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL3, "abc", "ghi");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, MoreSpecificNestedCookie) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL2, "abc", "ghi");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, MoreSpecificNestedCookieWithSameValue) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

TEST_F(CookieStoreIOSWithBackend, RemoveCallback) {
  CookieStoreIOS::SwitchSynchronizedStore(nullptr, store_.get());
  backend_->RunLoadedCallback();
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL, "abc", "def");
  scoped_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "ghi");
  EXPECT_EQ(2U, cookies.size());
  // this deletes the callback
  handle.reset();
  SetSystemCookie(kTestCookieURL, "abc", "jkl");
  EXPECT_EQ(2U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  store_->UnSynchronize();
}

}  // namespace net
