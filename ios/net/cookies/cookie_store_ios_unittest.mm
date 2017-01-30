// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/cookies/cookie_store_ios_persistent.h"
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
  static std::unique_ptr<net::CookieStore> Create() {
    ClearCookies();
    return base::MakeUnique<CookieStoreIOS>(
        [NSHTTPCookieStorage sharedHTTPCookieStorage]);
  }

  static const bool supports_http_only = false;
  static const bool supports_non_dotted_domains = false;
  static const bool preserves_trailing_dots = false;
  static const bool filters_schemes = false;
  static const bool has_path_prefix_bug = true;
  static const int creation_time_granularity_in_ms = 1000;

  base::MessageLoop loop_;
};

struct InactiveCookieStoreIOSTestTraits {
  static std::unique_ptr<net::CookieStore> Create() {
    return base::MakeUnique<CookieStoreIOSPersistent>(nullptr);
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

}  // namespace net

namespace net {

INSTANTIATE_TYPED_TEST_CASE_P(CookieStoreIOS,
                              CookieStoreTest,
                              CookieStoreIOSTestTraits);

INSTANTIATE_TYPED_TEST_CASE_P(InactiveCookieStoreIOS,
                              CookieStoreTest,
                              InactiveCookieStoreIOSTestTraits);

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
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
    net::CookieOptions options;
    options.set_include_httponly();

    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        kTestCookieURL, "a=b", base::Time::Now(), options));
    cookies.push_back(std::move(cookie));

    // Some canonical cookies cannot be converted into System cookies, for
    // example if value is not valid utf8. Such cookies are ignored.
    std::unique_ptr<net::CanonicalCookie> bad_canonical_cookie(
        net::CanonicalCookie::Create(GURL("http://domain/"), "name",
                                     "\x81r\xe4\xbd\xa0\xe5\xa5\xbd",
                                     std::string(), "/path/",
                                     base::Time(),  // creation
                                     base::Time(),  // expires
                                     false,         // secure
                                     false,         // httponly
                                     net::CookieSameSite::DEFAULT_MODE,
                                     net::COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(std::move(bad_canonical_cookie));
    loaded_callback_.Run(std::move(cookies));
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
                         net::CookieStore::ChangeCause cause) {
  DCHECK(out_cookies);
  out_cookies->push_back(cookie);
  if (out_removes)
    out_removes->push_back(net::CookieStore::ChangeCauseIsDeletion(cause));
}

void IgnoreBoolean(bool ignored) {
}

void IgnoreString(const std::string& ignored) {
}

}  // namespace

// Sets a cookie.
void SetCookie(const std::string& cookie_line,
               const GURL& url,
               net::CookieStore* store) {
  net::CookieOptions options;
  options.set_include_httponly();
  store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                   base::Bind(&IgnoreBoolean));
  net::CookieStoreIOS::NotifySystemCookiesChanged();
  // Wait until the flush is posted.
  base::RunLoop().RunUntilIdle();
}

// Test fixture to exersize net::CookieStoreIOS created with
// TestPersistentCookieStore backend and not synchronized with
// NSHTTPCookieStorage.
class NotSynchronizedCookieStoreIOSWithBackend : public testing::Test {
 public:
  NotSynchronizedCookieStoreIOSWithBackend()
      : kTestCookieURL("http://foo.google.com/bar"),
        backend_(new TestPersistentCookieStore),
        store_(base::MakeUnique<net::CookieStoreIOSPersistent>(backend_.get()))
  {
    cookie_changed_callback_ = store_->AddCallbackForCookie(
        kTestCookieURL, "abc",
        base::Bind(&RecordCookieChanges, &cookies_changed_, &cookies_removed_));
  }

  ~NotSynchronizedCookieStoreIOSWithBackend() override {}

  // Gets the cookies. |callback| will be called on completion.
  void GetCookies(const net::CookieStore::GetCookiesCallback& callback) {
    net::CookieOptions options;
    options.set_include_httponly();
    store_->GetCookiesWithOptionsAsync(kTestCookieURL, options, callback);
  }

  // Sets a cookie.
  void SetCookie(const std::string& cookie_line) {
    ::SetCookie(cookie_line, kTestCookieURL, store_.get());
  }

 private:
  const GURL kTestCookieURL;

 protected:
  base::MessageLoop loop_;
  scoped_refptr<TestPersistentCookieStore> backend_;
  std::unique_ptr<net::CookieStoreIOS> store_;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription>
      cookie_changed_callback_;
  std::vector<net::CanonicalCookie> cookies_changed_;
  std::vector<bool> cookies_removed_;
};

// Test fixture to exersize net::CookieStoreIOS created without backend and
// synchronized with |[NSHTTPCookieStorage sharedHTTPCookieStorage]|.
class SynchronizedCookieStoreIOS : public testing::Test {
 public:
  SynchronizedCookieStoreIOS()
      : kTestCookieURL("http://foo.google.com/bar"),
        kTestCookieURL2("http://foo.google.com/baz"),
        kTestCookieURL3("http://foo.google.com"),
        kTestCookieURL4("http://bar.google.com/bar"),
        backend_(new TestPersistentCookieStore),
        store_(base::MakeUnique<net::CookieStoreIOS>(
                [NSHTTPCookieStorage sharedHTTPCookieStorage])) {
    cookie_changed_callback_ = store_->AddCallbackForCookie(
        kTestCookieURL, "abc",
        base::Bind(&RecordCookieChanges, &cookies_changed_, &cookies_removed_));
  }

  ~SynchronizedCookieStoreIOS() override {}

  // Gets the cookies. |callback| will be called on completion.
  void GetCookies(const net::CookieStore::GetCookiesCallback& callback) {
    net::CookieOptions options;
    options.set_include_httponly();
    store_->GetCookiesWithOptionsAsync(kTestCookieURL, options, callback);
  }

  // Sets a cookie.
  void SetCookie(const std::string& cookie_line) {
    ::SetCookie(cookie_line, kTestCookieURL, store_.get());
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
    base::RunLoop().RunUntilIdle();
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
    base::RunLoop().RunUntilIdle();
  }

 protected:
  const GURL kTestCookieURL;
  const GURL kTestCookieURL2;
  const GURL kTestCookieURL3;
  const GURL kTestCookieURL4;

  base::MessageLoop loop_;
  scoped_refptr<TestPersistentCookieStore> backend_;
  std::unique_ptr<net::CookieStoreIOS> store_;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription>
      cookie_changed_callback_;
  std::vector<net::CanonicalCookie> cookies_changed_;
  std::vector<bool> cookies_removed_;
};

}  // namespace

namespace net {

TEST_F(NotSynchronizedCookieStoreIOSWithBackend, SetCookieCallsHook) {
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(0U, cookies_changed_.size());
  EXPECT_EQ(0U, cookies_removed_.size());
  backend_->RunLoadedCallback();
  base::RunLoop().RunUntilIdle();
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
}

TEST_F(SynchronizedCookieStoreIOS, SetCookieCallsHookWhenSynchronized) {
  GetCookies(base::Bind(&IgnoreString));
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
}

TEST_F(SynchronizedCookieStoreIOS, DeleteCallsHook) {
  GetCookies(base::Bind(&IgnoreString));
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  EXPECT_EQ(1U, cookies_removed_.size());
  store_->DeleteCookieAsync(kTestCookieURL, "abc",
                            base::Bind(&IgnoreBoolean, false));
  CookieStoreIOS::NotifySystemCookiesChanged();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SynchronizedCookieStoreIOS, SameValueDoesNotCallHook) {
  GetCookieCallback callback;
  GetCookies(base::Bind(&IgnoreString));
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
}

TEST(CookieStoreIOS, GetAllCookiesForURLAsync) {
  base::MessageLoop loop;
  const GURL kTestCookieURL("http://foo.google.com/bar");
  ClearCookies();
  std::unique_ptr<CookieStoreIOS> cookie_store(base::MakeUnique<CookieStoreIOS>(
      [NSHTTPCookieStorage sharedHTTPCookieStorage]));

  // Add a cookie.
  net::CookieOptions options;
  options.set_include_httponly();
  cookie_store->SetCookieWithOptionsAsync(
      kTestCookieURL, "a=b", options, net::CookieStore::SetCookiesCallback());
  // Check we can get the cookie.
  GetAllCookiesCallback callback;
  cookie_store->GetAllCookiesForURLAsync(
      kTestCookieURL,
      base::Bind(&GetAllCookiesCallback::Run, base::Unretained(&callback)));
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1u, callback.cookie_list().size());
  net::CanonicalCookie cookie = callback.cookie_list()[0];
  EXPECT_EQ("a", cookie.Name());
  EXPECT_EQ("b", cookie.Value());
}

// Tests that cookies can be read before the backend is loaded.
TEST_F(NotSynchronizedCookieStoreIOSWithBackend, NotSynchronized) {
  // Start fetching the cookie.
  GetCookieCallback callback;
  GetCookies(base::Bind(&GetCookieCallback::Run, base::Unretained(&callback)));
  // Backend loading completes.
  backend_->RunLoadedCallback();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("a=b", callback.cookie_line());
}

TEST_F(SynchronizedCookieStoreIOS, NoInitialNotifyWithNoCookie) {
  std::vector<net::CanonicalCookie> cookies;
  store_->AddCallbackForCookie(
      kTestCookieURL, "abc",
      base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
}

TEST_F(SynchronizedCookieStoreIOS, NoInitialNotifyWithSystemCookie) {
  SetSystemCookie(kTestCookieURL, "abc", "def");
  std::vector<net::CanonicalCookie> cookies;
  store_->AddCallbackForCookie(
      kTestCookieURL, "abc",
      base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, NotifyOnAdd) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
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
}

TEST_F(SynchronizedCookieStoreIOS, NotifyOnChange) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
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
}

TEST_F(SynchronizedCookieStoreIOS, NotifyOnDelete) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  SetSystemCookie(kTestCookieURL, "abc", "def");
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
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
}

TEST_F(SynchronizedCookieStoreIOS, NoNotifyOnNoChange) {
  std::vector<net::CanonicalCookie> cookies;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  SetSystemCookie(kTestCookieURL, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, MultipleNotifies) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> cookies2;
  std::vector<net::CanonicalCookie> cookies3;
  std::vector<net::CanonicalCookie> cookies4;
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle2 =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies2, nullptr));
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle3 =
      store_->AddCallbackForCookie(
          kTestCookieURL3, "abc",
          base::Bind(&RecordCookieChanges, &cookies3, nullptr));
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle4 =
      store_->AddCallbackForCookie(
          kTestCookieURL4, "abc",
          base::Bind(&RecordCookieChanges, &cookies4, nullptr));
  SetSystemCookie(kTestCookieURL, "abc", "def");
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  SetSystemCookie(kTestCookieURL4, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, cookies2.size());
  EXPECT_EQ(0U, cookies3.size());
  EXPECT_EQ(1U, cookies4.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
  DeleteSystemCookie(kTestCookieURL2, "abc");
  DeleteSystemCookie(kTestCookieURL3, "abc");
  DeleteSystemCookie(kTestCookieURL4, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, LessSpecificNestedCookie) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL3, "abc", "ghi");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, MoreSpecificNestedCookie) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL2, "abc", "ghi");
  EXPECT_EQ(2U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, MoreSpecificNestedCookieWithSameValue) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL3, "abc", "def");
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
      store_->AddCallbackForCookie(
          kTestCookieURL2, "abc",
          base::Bind(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURL2, "abc", "def");
  EXPECT_EQ(2U, cookies.size());
  DeleteSystemCookie(kTestCookieURL, "abc");
}

TEST_F(SynchronizedCookieStoreIOS, RemoveCallback) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURL, "abc", "def");
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> handle =
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
}

}  // namespace net
