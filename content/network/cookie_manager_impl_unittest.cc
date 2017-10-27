// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/cookie_manager_impl.h"

#include <algorithm>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test infrastructure outline:
//      # Classes
//      * SynchronousMojoCookieWrapper: Takes a network::mojom::CookieManager at
//        construction; exposes synchronous interfaces that wrap the
//        network::mojom::CookieManager async interfaces to make testing easier.
//      * CookieChangeNotificationImpl: Test class implementing
//        the CookieChangeNotification interface and recording
//        incoming messages on it.
//      * CookieManagerImplTest: Test base class.  Automatically sets up
//        a cookie store, a cookie service wrapping it, a mojo pipe
//        connected to the server, and the cookie service implemented
//        by the other end of the pipe.
//
//      # Functions
//      * CompareCanonicalCookies: Comparison function to make it easy to
//        sort cookie list responses from the network::mojom::CookieManager.

namespace content {

// Wraps a network::mojom::CookieManager in synchronous, blocking calls to make
// it easier to test.
class SynchronousCookieManager {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit SynchronousCookieManager(
      network::mojom::CookieManager* cookie_service)
      : cookie_service_(cookie_service) {}
  ~SynchronousCookieManager() {}

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies;
    cookie_service_->GetAllCookies(base::BindOnce(
        &SynchronousCookieManager::GetCookiesCallback, &run_loop, &cookies));
    run_loop.Run();
    return cookies;
  }

  std::vector<net::CanonicalCookie> GetCookieList(const GURL& url,
                                                  net::CookieOptions options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies;
    cookie_service_->GetCookieList(
        url, options,
        base::BindOnce(&SynchronousCookieManager::GetCookiesCallback, &run_loop,
                       &cookies));

    run_loop.Run();
    return cookies;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only) {
    base::RunLoop run_loop;
    bool result = false;
    cookie_service_->SetCanonicalCookie(
        cookie, secure_source, modify_http_only,
        base::BindOnce(&SynchronousCookieManager::SetCookieCallback, &run_loop,
                       &result));
    run_loop.Run();
    return result;
  }

  uint32_t DeleteCookies(network::mojom::CookieDeletionFilter filter) {
    base::RunLoop run_loop;
    uint32_t num_deleted = 0u;
    network::mojom::CookieDeletionFilterPtr filter_ptr =
        network::mojom::CookieDeletionFilter::New(filter);

    cookie_service_->DeleteCookies(
        std::move(filter_ptr),
        base::BindOnce(&SynchronousCookieManager::DeleteCookiesCallback,
                       &run_loop, &num_deleted));
    run_loop.Run();
    return num_deleted;
  }

  // No need to wrap RequestNotification and CloneInterface, since their use
  // is pure async.
 private:
  static void GetCookiesCallback(
      base::RunLoop* run_loop,
      std::vector<net::CanonicalCookie>* cookies_out,
      const std::vector<net::CanonicalCookie>& cookies) {
    *cookies_out = cookies;
    run_loop->Quit();
  }

  static void SetCookieCallback(base::RunLoop* run_loop,
                                bool* result_out,
                                bool result) {
    *result_out = result;
    run_loop->Quit();
  }

  static void DeleteCookiesCallback(base::RunLoop* run_loop,
                                    uint32_t* num_deleted_out,
                                    uint32_t num_deleted) {
    *num_deleted_out = num_deleted;
    run_loop->Quit();
  }

  static void RequestNotificationCallback(
      base::RunLoop* run_loop,
      network::mojom::CookieChangeNotificationRequest* request_out,
      network::mojom::CookieChangeNotificationRequest request) {
    *request_out = std::move(request);
    run_loop->Quit();
  }

  network::mojom::CookieManager* cookie_service_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCookieManager);
};

class CookieManagerImplTest : public testing::Test {
 public:
  CookieManagerImplTest()
      : connection_error_seen_(false),
        cookie_monster_(nullptr, nullptr),
        cookie_service_(std::make_unique<CookieManagerImpl>(&cookie_monster_)) {
    cookie_service_->AddRequest(mojo::MakeRequest(&cookie_service_ptr_));
    service_wrapper_ =
        std::make_unique<SynchronousCookieManager>(cookie_service_ptr_.get());
    cookie_service_ptr_.set_connection_error_handler(base::BindOnce(
        &CookieManagerImplTest::OnConnectionError, base::Unretained(this)));
  }
  ~CookieManagerImplTest() override {}

  // Tear down the remote service.
  void NukeService() { cookie_service_.reset(); }

  // Set a canonical cookie directly into the store.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool can_modify_httponly) {
    net::ResultSavingCookieCallback<bool> callback;
    cookie_monster_.SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie), secure_source,
        can_modify_httponly,
        base::BindOnce(&net::ResultSavingCookieCallback<bool>::Run,
                       base::Unretained(&callback)));
    callback.WaitUntilDone();
    return callback.result();
  }

  net::CookieStore* cookie_store() { return &cookie_monster_; }

  CookieManagerImpl* service_impl() const { return cookie_service_.get(); }

  // Return the cookie service at the client end of the mojo pipe.
  network::mojom::CookieManager* cookie_service_client() {
    return cookie_service_ptr_.get();
  }

  // Synchronous wrapper
  SynchronousCookieManager* service_wrapper() { return service_wrapper_.get(); }

  bool connection_error_seen() const { return connection_error_seen_; }

 private:
  void OnConnectionError() { connection_error_seen_ = true; }

  bool connection_error_seen_;

  base::MessageLoopForIO message_loop_;
  net::CookieMonster cookie_monster_;
  std::unique_ptr<content::CookieManagerImpl> cookie_service_;
  network::mojom::CookieManagerPtr cookie_service_ptr_;
  std::unique_ptr<SynchronousCookieManager> service_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(CookieManagerImplTest);
};

namespace {

bool CompareCanonicalCookies(const net::CanonicalCookie& c1,
                             const net::CanonicalCookie& c2) {
  return c1.FullCompare(c2);
}

}  // anonymous namespace

// Test the GetAllCookies accessor.  Also tests that canonical
// cookies come out of the store unchanged.
TEST_F(CookieManagerImplTest, GetAllCookies) {
  base::Time before_creation(base::Time::Now());

  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  base::Time after_creation(base::Time::Now());

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(4u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());
  EXPECT_EQ("foo_host", cookies[0].Domain());
  EXPECT_EQ("/", cookies[0].Path());
  EXPECT_LT(before_creation, cookies[0].CreationDate());
  EXPECT_LE(cookies[0].CreationDate(), after_creation);
  EXPECT_EQ(cookies[0].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[0].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[0].IsPersistent());
  EXPECT_FALSE(cookies[0].IsSecure());
  EXPECT_FALSE(cookies[0].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[0].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[0].Priority());

  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("D", cookies[1].Value());
  EXPECT_EQ("foo_host2", cookies[1].Domain());
  EXPECT_EQ("/with/path", cookies[1].Path());
  EXPECT_LT(before_creation, cookies[1].CreationDate());
  EXPECT_LE(cookies[1].CreationDate(), after_creation);
  EXPECT_EQ(cookies[1].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[1].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[1].IsPersistent());
  EXPECT_FALSE(cookies[1].IsSecure());
  EXPECT_FALSE(cookies[1].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[1].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[1].Priority());

  EXPECT_EQ("HttpOnly", cookies[2].Name());
  EXPECT_EQ("F", cookies[2].Value());
  EXPECT_EQ("foo_host", cookies[2].Domain());
  EXPECT_EQ("/with/path", cookies[2].Path());
  EXPECT_LT(before_creation, cookies[2].CreationDate());
  EXPECT_LE(cookies[2].CreationDate(), after_creation);
  EXPECT_EQ(cookies[2].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[2].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[2].IsPersistent());
  EXPECT_FALSE(cookies[2].IsSecure());
  EXPECT_TRUE(cookies[2].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[2].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[2].Priority());

  EXPECT_EQ("Secure", cookies[3].Name());
  EXPECT_EQ("E", cookies[3].Value());
  EXPECT_EQ("foo_host", cookies[3].Domain());
  EXPECT_EQ("/with/path", cookies[3].Path());
  EXPECT_LT(before_creation, cookies[3].CreationDate());
  EXPECT_LE(cookies[3].CreationDate(), after_creation);
  EXPECT_EQ(cookies[3].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[3].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[3].IsPersistent());
  EXPECT_TRUE(cookies[3].IsSecure());
  EXPECT_FALSE(cookies[3].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[3].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[3].Priority());
}

TEST_F(CookieManagerImplTest, GetCookieList) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host/with/path"), net::CookieOptions());

  EXPECT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());

  EXPECT_EQ("Secure", cookies[1].Name());
  EXPECT_EQ("E", cookies[1].Value());
}

TEST_F(CookieManagerImplTest, GetCookieListHttpOnly) {
  // Create an httponly and a non-httponly cookie.
  bool result;
  result = SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/true,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);

  // Retrieve without httponly cookies (default)
  net::CookieOptions options;
  EXPECT_TRUE(options.exclude_httponly());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host/with/path"), options);
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("C", cookies[0].Name());

  // Retrieve with httponly cookies.
  options.set_include_httponly();
  cookies = service_wrapper()->GetCookieList(GURL("https://foo_host/with/path"),
                                             options);
  EXPECT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
}

TEST_F(CookieManagerImplTest, GetCookieListSameSite) {
  // Create an unrestricted, a lax, and a strict cookie.
  bool result;
  result = SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie("C", "D", "foo_host", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie("E", "F", "foo_host", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);

  // Retrieve only unrestricted cookies.
  net::CookieOptions options;
  EXPECT_EQ(net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE,
            options.same_site_cookie_mode());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host/with/path"), options);
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());

  // Retrieve unrestricted and lax cookies.
  options.set_same_site_cookie_mode(
      net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
  cookies = service_wrapper()->GetCookieList(GURL("https://foo_host/with/path"),
                                             options);
  EXPECT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());

  // Retrieve everything.
  options.set_same_site_cookie_mode(
      net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  cookies = service_wrapper()->GetCookieList(GURL("https://foo_host/with/path"),
                                             options);
  EXPECT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("E", cookies[2].Name());
}

TEST_F(CookieManagerImplTest, GetCookieListAccessTime) {
  bool result = SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  ASSERT_TRUE(result);

  // Get the cookie without updating the access time and check
  // the access time is null.
  net::CookieOptions options;
  options.set_do_not_update_access_time();
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host/with/path"), options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_TRUE(cookies[0].LastAccessDate().is_null());

  // Get the cookie updating the access time and check
  // that it's a valid value.
  base::Time start(base::Time::Now());
  options.set_update_access_time();
  cookies = service_wrapper()->GetCookieList(GURL("https://foo_host/with/path"),
                                             options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_FALSE(cookies[0].LastAccessDate().is_null());
  EXPECT_GE(cookies[0].LastAccessDate(), start);
  EXPECT_LE(cookies[0].LastAccessDate(), base::Time::Now());
}

TEST_F(CookieManagerImplTest, DeleteThroughSet) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  base::Time yesterday = base::Time::Now() - base::TimeDelta::FromDays(1);
  EXPECT_TRUE(service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie("A", "E", "foo_host", "/", base::Time(), yesterday,
                           base::Time(), /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      false, false));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("C", cookies[0].Name());
  EXPECT_EQ("D", cookies[0].Value());

  EXPECT_EQ("HttpOnly", cookies[1].Name());
  EXPECT_EQ("F", cookies[1].Value());

  EXPECT_EQ("Secure", cookies[2].Name());
  EXPECT_EQ("E", cookies[2].Value());
}

TEST_F(CookieManagerImplTest, ConfirmSecureSetFails) {
  EXPECT_FALSE(service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          "N", "O", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      false, false));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerImplTest, ConfirmHttpOnlySetFails) {
  EXPECT_FALSE(service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          "N", "O", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/true,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      false, false));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerImplTest, ConfirmHttpOnlyOverwriteFails) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_FALSE(service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "Nope", "foo_host", "/with/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      false, false));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("HttpOnly", cookies[0].Name());
  EXPECT_EQ("F", cookies[0].Value());
}

TEST_F(CookieManagerImplTest, DeleteEverything) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "foo_host", "/with/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  EXPECT_EQ(4u, service_wrapper()->DeleteCookies(filter));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerImplTest, DeleteByTime) {
  base::Time now(base::Time::Now());

  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "foo_host", "/", now - base::TimeDelta::FromMinutes(60),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "foo_host", "/", now - base::TimeDelta::FromMinutes(120),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "foo_host", "/", now - base::TimeDelta::FromMinutes(180),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.created_after_time = now - base::TimeDelta::FromMinutes(150);
  filter.created_before_time = now - base::TimeDelta::FromMinutes(90);
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

TEST_F(CookieManagerImplTest, DeleteByExcludingDomains) {
  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "foo_host2", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "foo_host3", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.excluding_domains = std::vector<std::string>();
  filter.excluding_domains->push_back("foo_host2");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A2", cookies[0].Name());
}

TEST_F(CookieManagerImplTest, DeleteByIncludingDomains) {
  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "foo_host2", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "foo_host3", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("foo_host1");
  filter.including_domains->push_back("foo_host3");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A2", cookies[0].Name());
}

// Confirm deletion is based on eTLD+1
TEST_F(CookieManagerImplTest, DeleteDetails_eTLD) {
  // Two domains on diferent levels of the same eTLD both get deleted.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "www.example.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "www.nonexample.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.com");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("A3", cookies[0].Name());
  filter = network::mojom::CookieDeletionFilter();
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  // Same thing happens on an eTLD+1 which isn't a TLD+1.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "www.example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.co.uk");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  cookies = service_wrapper()->GetAllCookies();
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("A3", cookies[0].Name());
  filter = network::mojom::CookieDeletionFilter();
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  // Deletion of a second level domain that's an eTLD doesn't delete any
  // subdomains.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "www.example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("co.uk");
  EXPECT_EQ(0u, service_wrapper()->DeleteCookies(filter));
  cookies = service_wrapper()->GetAllCookies();
  EXPECT_EQ(3u, cookies.size());
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A2", cookies[1].Name());
  EXPECT_EQ("A3", cookies[2].Name());
}

// Confirm deletion ignores host/domain distinction.
TEST_F(CookieManagerImplTest, DeleteDetails_HostDomain) {
  // Create four cookies: A host (no leading .) and domain cookie
  // (leading .) for each of two separate domains.  Confirm that the
  // filter deletes both of one domain and leaves the other alone.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "foo_host.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", ".foo_host.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "bar.host.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A4", "val", ".bar.host.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("foo_host.com");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  EXPECT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A3", cookies[0].Name());
  EXPECT_EQ("A4", cookies[1].Name());
}

TEST_F(CookieManagerImplTest, DeleteDetails_eTLDvsPrivateRegistry) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "random.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "sub.domain.random.co.uk", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "random.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A4", "val", "random", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A5", "val", "normal.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("random.co.uk");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A3", cookies[0].Name());
  EXPECT_EQ("A4", cookies[1].Name());
  EXPECT_EQ("A5", cookies[2].Name());
}

TEST_F(CookieManagerImplTest, DeleteDetails_PrivateRegistry) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "privatedomain", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          // Will not actually be treated as a private domain as it's under
          // .com.
          "A2", "val", "privatedomain.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          // Will not actually be treated as a private domain as it's two
          // level
          "A3", "val", "subdomain.privatedomain", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("privatedomain");
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A2", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

// Test to probe and make sure the attributes that deletion should ignore
// don't affect the results.
TEST_F(CookieManagerImplTest, DeleteDetails_IgnoredFields) {
  // Simple deletion filter
  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.com");

  // Set two cookies for each ignored field, one that will be deleted by the
  // above filter, and one that won't, with values unused in other tests
  // for the ignored field.  Secure & httponly are tested in
  // DeleteDetails_Consumer below, and expiration does affect deletion
  // through the persistence distinction.

  // Value
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A01", "RandomValue", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A02", "RandomValue", "canonical.com", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Path
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A03", "val", "example.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A04", "val", "canonical.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Last_access
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A05", "val", "example.com", "/",
          base::Time::Now() - base::TimeDelta::FromDays(3), base::Time(),
          base::Time::Now() - base::TimeDelta::FromDays(3), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A06", "val", "canonical.com", "/",
          base::Time::Now() - base::TimeDelta::FromDays(3), base::Time(),
          base::Time::Now() - base::TimeDelta::FromDays(3), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Same_site
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A07", "val", "example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A08", "val", "canonical.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Priority
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A09", "val", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_HIGH),
      true, true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A10", "val", "canonical.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_HIGH),
      true, true));

  // Use the filter and make sure the result is the expected set.
  EXPECT_EQ(5u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(5u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A02", cookies[0].Name());
  EXPECT_EQ("A04", cookies[1].Name());
  EXPECT_EQ("A06", cookies[2].Name());
  EXPECT_EQ("A08", cookies[3].Name());
  EXPECT_EQ("A10", cookies[4].Name());
}

// A set of tests specified by the only consumer of this interface
// (BrowsingDataFilterBuilderImpl).
TEST_F(CookieManagerImplTest, DeleteDetails_Consumer) {
  const char* filter_domains[] = {
      "google.com",

      // sp.nom.br is an eTLD, so this is a regular valid registrable domain,
      // just like google.com.
      "website.sp.nom.br",

      // This domain will also not be found in registries, and since it has only
      // one component, it will not be recognized as a valid registrable domain.
      "fileserver",

      // This domain will not be found in registries. It will be assumed that
      // it belongs to an unknown registry, and since it has two components,
      // they will be treated as the second level domain and TLD. Most
      // importantly, it will NOT be treated as a subdomain of "fileserver".
      "second-level-domain.fileserver",

      // IP addresses are supported.
      "192.168.1.1",
  };
  network::mojom::CookieDeletionFilter test_filter;
  test_filter.including_domains = std::vector<std::string>();
  for (int i = 0; i < static_cast<int>(arraysize(filter_domains)); ++i)
    test_filter.including_domains->push_back(filter_domains[i]);

  struct TestCase {
    std::string domain;
    std::string path;
    bool expect_delete;
  } test_cases[] = {
      // We match any URL on the specified domains.
      {"www.google.com", "/foo/bar", true},
      {"www.sub.google.com", "/foo/bar", true},
      {"sub.google.com", "/", true},
      {"www.sub.google.com", "/foo/bar", true},
      {"website.sp.nom.br", "/", true},
      {"www.website.sp.nom.br", "/", true},
      {"192.168.1.1", "/", true},

      // Internal hostnames do not have subdomains.
      {"fileserver", "/", true},
      {"fileserver", "/foo/bar", true},
      {"website.fileserver", "/foo/bar", false},

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"second-level-domain.fileserver", "/foo", true},
      {"www.second-level-domain.fileserver", "/index.html", true},

      // Different domains.
      {"www.youtube.com", "/", false},
      {"www.google.net", "/", false},
      {"192.168.1.2", "/", false},

      // Check both a bare eTLD.
      {"sp.nom.br", "/", false},
  };

  network::mojom::CookieDeletionFilter clear_filter;
  for (int i = 0; i < static_cast<int>(arraysize(test_cases)); ++i) {
    TestCase& test_case(test_cases[i]);

    // Clear store.
    service_wrapper()->DeleteCookies(clear_filter);

    // IP addresses and internal hostnames can only be host cookies.
    bool exclude_domain_cookie =
        (GURL("http://" + test_case.domain).HostIsIPAddress() ||
         test_case.domain.find(".") == std::string::npos);

    // Standard cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A1", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
        true, true));

    if (!exclude_domain_cookie) {
      // Host cookie
      EXPECT_TRUE(SetCanonicalCookie(
          net::CanonicalCookie(
              "A2", "val", "." + test_cases[i].domain, test_cases[i].path,
              base::Time(), base::Time(), base::Time(), /*secure=*/false,
              /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
              net::COOKIE_PRIORITY_MEDIUM),
          true, true));
    }

    // Httponly cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A3", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/true,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
        true, true));

    // Httponly and secure cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A4", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/true,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
        true, true));

    const uint32_t number_cookies = exclude_domain_cookie ? 3u : 4u;
    EXPECT_EQ(number_cookies, service_wrapper()->GetAllCookies().size());
    EXPECT_EQ(test_cases[i].expect_delete ? number_cookies : 0u,
              service_wrapper()->DeleteCookies(test_filter))
        << "Case " << i << " domain " << test_cases[i].domain << " path "
        << test_cases[i].path << " expect "
        << (test_cases[i].expect_delete ? "delete" : "keep");
    EXPECT_EQ(test_cases[i].expect_delete ? 0u : number_cookies,
              service_wrapper()->GetAllCookies().size());
  }
}

TEST_F(CookieManagerImplTest, DeleteBySessionStatus) {
  base::Time now(base::Time::Now());

  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "foo_host", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "foo_host", "/", base::Time(),
                           now + base::TimeDelta::FromDays(1), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "foo_host", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  network::mojom::CookieDeletionFilter filter;
  filter.session_control =
      network::mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

TEST_F(CookieManagerImplTest, DeleteByAll) {
  base::Time now(base::Time::Now());

  // Add a lot of cookies, only one of which will be deleted by the filter.
  // Filter will be:
  //    * Between two and four days ago.
  //    * Including domains: no.com and nope.com
  //    * Excluding domains: no.com and yes.com (excluding wins on no.com
  //      because of ANDing)
  //    * Persistent cookies.
  // The archetypal cookie (which will be deleted) will satisfy all of
  // these filters (2 days old, nope.com, persistent).
  // Each of the other four cookies will vary in a way that will take it out
  // of being deleted by one of the filter.

  network::mojom::CookieDeletionFilter filter;
  filter.created_after_time = now - base::TimeDelta::FromDays(4);
  filter.created_before_time = now - base::TimeDelta::FromDays(2);
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("no.com");
  filter.including_domains->push_back("nope.com");
  filter.excluding_domains = std::vector<std::string>();
  filter.excluding_domains->push_back("no.com");
  filter.excluding_domains->push_back("yes.com");
  filter.session_control =
      network::mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;

  // Architectypal cookie:
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val", "nope.com", "/", now - base::TimeDelta::FromDays(3),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Too old cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "nope.com", "/", now - base::TimeDelta::FromDays(5),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Too young cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "nope.com", "/", now - base::TimeDelta::FromDays(1),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Not in including_domains.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A4", "val", "other.com", "/", now - base::TimeDelta::FromDays(3),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // In excluding_domains.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A5", "val", "no.com", "/", now - base::TimeDelta::FromDays(3),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  // Session
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A6", "val", "nope.com", "/", now - base::TimeDelta::FromDays(3),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(5u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A2", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
  EXPECT_EQ("A4", cookies[2].Name());
  EXPECT_EQ("A5", cookies[3].Name());
  EXPECT_EQ("A6", cookies[4].Name());
}

// Receives and records notifications from the network::mojom::CookieManager.
class CookieChangeNotificationImpl
    : public network::mojom::CookieChangeNotification {
 public:
  struct Notification {
    Notification(const net::CanonicalCookie& cookie_in,
                 network::mojom::CookieChangeCause cause_in) {
      cookie = cookie_in;
      cause = cause_in;
    }

    net::CanonicalCookie cookie;
    network::mojom::CookieChangeCause cause;
  };

  CookieChangeNotificationImpl(
      network::mojom::CookieChangeNotificationRequest request)
      : run_loop_(nullptr), binding_(this, std::move(request)) {}

  void WaitForSomeNotification() {
    if (!notifications_.empty())
      return;
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  // Adds existing notifications to passed in vector.
  void GetCurrentNotifications(std::vector<Notification>* notifications) {
    notifications->insert(notifications->end(), notifications_.begin(),
                          notifications_.end());
    notifications_.clear();
  }

  // network::mojom::CookieChangesNotification
  void OnCookieChanged(const net::CanonicalCookie& cookie,
                       network::mojom::CookieChangeCause cause) override {
    notifications_.push_back(Notification(cookie, cause));
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::vector<Notification> notifications_;

  // Loop to signal on receiving a notification if not null.
  base::RunLoop* run_loop_;

  mojo::Binding<network::mojom::CookieChangeNotification> binding_;
};

TEST_F(CookieManagerImplTest, Notification) {
  GURL notification_url("http://www.testing.com/pathele");
  std::string notification_domain("testing.com");
  std::string notification_name("Cookie_Name");
  network::mojom::CookieChangeNotificationPtr ptr;
  network::mojom::CookieChangeNotificationRequest request(
      mojo::MakeRequest(&ptr));

  CookieChangeNotificationImpl notification_impl(std::move(request));

  cookie_service_client()->RequestNotification(
      notification_url, notification_name, std::move(ptr));

  std::vector<CookieChangeNotificationImpl::Notification> notifications;
  notification_impl.GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  // Set a cookie that doesn't match the above notification request in name
  // and confirm it doesn't produce a notification.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          "DifferentName", "val", notification_url.host(), "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true);
  base::RunLoop().RunUntilIdle();
  notification_impl.GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  // Set a cookie that doesn't match the above notification request in url
  // and confirm it doesn't produce a notification.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          notification_name, "val", "www.other.host", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true);

  base::RunLoop().RunUntilIdle();
  notification_impl.GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  // Insert a cookie that does match.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          notification_name, "val", notification_url.host(), "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true);

  // Expect asynchrony
  notification_impl.GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  // Expect notification
  notification_impl.WaitForSomeNotification();
  notification_impl.GetCurrentNotifications(&notifications);
  EXPECT_EQ(1u, notifications.size());
  EXPECT_EQ(notification_name, notifications[0].cookie.Name());
  EXPECT_EQ(notification_url.host(), notifications[0].cookie.Domain());
  EXPECT_EQ(network::mojom::CookieChangeCause::INSERTED,
            notifications[0].cause);
  notifications.clear();

  // Delete all cookies matching the domain.  This includes one for which
  // a notification will be generated, and one for which a notification
  // will not be generated.
  network::mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back(notification_domain);
  // If this test fails, it may indicate a problem which will lead to
  // no notifications being generated and the test hanging, so assert.
  ASSERT_EQ(2u, service_wrapper()->DeleteCookies(filter));

  // The notification may already have arrived, or it may arrive in the future.
  notification_impl.WaitForSomeNotification();
  notification_impl.GetCurrentNotifications(&notifications);
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notification_name, notifications[0].cookie.Name());
  EXPECT_EQ(notification_url.host(), notifications[0].cookie.Domain());
  EXPECT_EQ(network::mojom::CookieChangeCause::EXPLICIT,
            notifications[0].cause);
}

// Confirm the service operates properly if a returned notification interface
// is destroyed.
TEST_F(CookieManagerImplTest, NotificationRequestDestroyed) {
  // Create two identical notification interfaces.
  GURL notification_url("http://www.testing.com/pathele");
  std::string notification_name("Cookie_Name");

  network::mojom::CookieChangeNotificationPtr ptr1;
  network::mojom::CookieChangeNotificationRequest request1(
      mojo::MakeRequest(&ptr1));
  std::unique_ptr<CookieChangeNotificationImpl> notification_impl1(
      std::make_unique<CookieChangeNotificationImpl>(std::move(request1)));
  cookie_service_client()->RequestNotification(
      notification_url, notification_name, std::move(ptr1));

  network::mojom::CookieChangeNotificationPtr ptr2;
  network::mojom::CookieChangeNotificationRequest request2(
      mojo::MakeRequest(&ptr2));
  std::unique_ptr<CookieChangeNotificationImpl> notification_impl2(
      std::make_unique<CookieChangeNotificationImpl>(std::move(request2)));
  cookie_service_client()->RequestNotification(
      notification_url, notification_name, std::move(ptr2));

  // Add a cookie and receive a notification on both interfaces.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          notification_name, "val", notification_url.host(), "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      true, true);

  std::vector<CookieChangeNotificationImpl::Notification> notifications;
  notification_impl1->GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  notification_impl2->GetCurrentNotifications(&notifications);
  EXPECT_EQ(0u, notifications.size());
  notifications.clear();

  notification_impl1->WaitForSomeNotification();
  notification_impl1->GetCurrentNotifications(&notifications);
  EXPECT_EQ(1u, notifications.size());
  notifications.clear();

  notification_impl2->WaitForSomeNotification();
  notification_impl2->GetCurrentNotifications(&notifications);
  EXPECT_EQ(1u, notifications.size());
  notifications.clear();
  EXPECT_EQ(2u, service_impl()->GetNotificationsBoundForTesting());

  // Destroy the first interface
  notification_impl1.reset();

  // Confirm the second interface can still receive notifications.
  network::mojom::CookieDeletionFilter filter;
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  notification_impl2->WaitForSomeNotification();
  notification_impl2->GetCurrentNotifications(&notifications);
  EXPECT_EQ(1u, notifications.size());
  notifications.clear();
  EXPECT_EQ(1u, service_impl()->GetNotificationsBoundForTesting());
}

// Confirm we get a connection error notification if the service dies.
TEST_F(CookieManagerImplTest, ServiceDestructVisible) {
  EXPECT_FALSE(connection_error_seen());
  NukeService();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connection_error_seen());
}

// Test service cloning.  Also confirm that the service notices if a client
// dies.
TEST_F(CookieManagerImplTest, CloningAndClientDestructVisible) {
  EXPECT_EQ(1u, service_impl()->GetClientsBoundForTesting());

  // Clone the interface.
  network::mojom::CookieManagerPtr new_ptr;
  cookie_service_client()->CloneInterface(mojo::MakeRequest(&new_ptr));

  SynchronousCookieManager new_wrapper(new_ptr.get());

  // Set a cookie on the new interface and make sure it's visible on the
  // old one.
  EXPECT_TRUE(new_wrapper.SetCanonicalCookie(
      net::CanonicalCookie(
          "X", "Y", "www.other.host", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      true, true));

  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("http://www.other.host/"), net::CookieOptions());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("X", cookies[0].Name());
  EXPECT_EQ("Y", cookies[0].Value());

  // After a synchronous round trip through the new client pointer, it
  // should be reflected in the bindings seen on the server.
  EXPECT_EQ(2u, service_impl()->GetClientsBoundForTesting());

  new_ptr.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, service_impl()->GetClientsBoundForTesting());
}

}  // namespace content
