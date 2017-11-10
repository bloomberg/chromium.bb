// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/restricted_cookie_manager.h"

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Synchronous proxies to a wrapped RestrictedCookieManager's methods.
class RestrictedCookieManagerSync {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit RestrictedCookieManagerSync(
      network::mojom::RestrictedCookieManager* cookie_service)
      : cookie_service_(cookie_service) {}
  ~RestrictedCookieManagerSync() {}

  std::vector<net::CanonicalCookie> GetAllForUrl(
      const GURL& url,
      const GURL& site_for_cookies,
      network::mojom::CookieManagerGetOptionsPtr options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies;
    cookie_service_->GetAllForUrl(
        url, site_for_cookies, std::move(options),
        base::BindOnce(&RestrictedCookieManagerSync::GetAllForUrlCallback,
                       &run_loop, &cookies));
    run_loop.Run();
    return cookies;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies) {
    base::RunLoop run_loop;
    bool result = false;
    cookie_service_->SetCanonicalCookie(
        cookie, url, site_for_cookies,
        base::BindOnce(&RestrictedCookieManagerSync::SetCanonicalCookieCallback,
                       &run_loop, &result));
    run_loop.Run();
    return result;
  }

 private:
  static void GetAllForUrlCallback(
      base::RunLoop* run_loop,
      std::vector<net::CanonicalCookie>* cookies_out,
      const std::vector<net::CanonicalCookie>& cookies) {
    *cookies_out = cookies;
    run_loop->Quit();
  }

  static void SetCanonicalCookieCallback(base::RunLoop* run_loop,
                                         bool* result_out,
                                         bool result) {
    *result_out = result;
    run_loop->Quit();
  }

  network::mojom::RestrictedCookieManager* cookie_service_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedCookieManagerSync);
};

class RestrictedCookieManagerTest : public testing::Test {
 public:
  RestrictedCookieManagerTest()
      : cookie_monster_(nullptr, nullptr),
        service_(std::make_unique<RestrictedCookieManager>(&cookie_monster_,
                                                           MSG_ROUTING_NONE,
                                                           MSG_ROUTING_NONE)),
        binding_(service_.get(), mojo::MakeRequest(&service_ptr_)) {
    sync_service_ =
        std::make_unique<RestrictedCookieManagerSync>(service_ptr_.get());
  }
  ~RestrictedCookieManagerTest() override {}

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

  // Simplified helper for SetCanonicalCookie.
  //
  // Creates a CanonicalCookie that is not secure, not http-only,
  // and not restricted to first parties. Crashes if the creation fails.
  void SetSessionCookie(const char* name,
                        const char* value,
                        const char* domain,
                        const char* path) {
    CHECK(SetCanonicalCookie(
        net::CanonicalCookie(name, value, domain, path, base::Time(),
                             base::Time(), base::Time(), /* secure = */ false,
                             /* httponly = */ false,
                             net::CookieSameSite::NO_RESTRICTION,
                             net::COOKIE_PRIORITY_DEFAULT),
        /* secure_source = */ true, /* can_modify_httponly = */ true));
  }

 protected:
  base::MessageLoopForIO message_loop_;
  net::CookieMonster cookie_monster_;
  std::unique_ptr<content::RestrictedCookieManager> service_;
  network::mojom::RestrictedCookieManagerPtr service_ptr_;
  mojo::Binding<network::mojom::RestrictedCookieManager> binding_;
  std::unique_ptr<RestrictedCookieManagerSync> sync_service_;
};

namespace {

bool CompareCanonicalCookies(const net::CanonicalCookie& c1,
                             const net::CanonicalCookie& c2) {
  return c1.FullCompare(c2);
}

}  // anonymous namespace

TEST_F(RestrictedCookieManagerTest, GetAllForUrlBlankFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = network::mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = network::mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());

  EXPECT_EQ("cookie-name-2", cookies[1].Name());
  EXPECT_EQ("cookie-value-2", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEmptyFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = network::mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = network::mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_EQ(0u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEqualsMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");

  auto options = network::mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = network::mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_EQ(1u, cookies.size());

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlStartsWithMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("cookie-name-2b", "cookie-value-2b", "example.com", "/");
  SetSessionCookie("cookie-name-3b", "cookie-value-3b", "example.com", "/");

  auto options = network::mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name-2";
  options->match_type = network::mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name-2", cookies[0].Name());
  EXPECT_EQ("cookie-value-2", cookies[0].Value());

  EXPECT_EQ("cookie-name-2b", cookies[1].Name());
  EXPECT_EQ("cookie-value-2b", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookie) {
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      net::CanonicalCookie(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      GURL("http://example.com/test/"), GURL("http://example.com")));

  auto options = network::mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = network::mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_EQ(1u, cookies.size());

  EXPECT_EQ("new-name", cookies[0].Name());
  EXPECT_EQ("new-value", cookies[0].Value());
}

}  // namespace content
