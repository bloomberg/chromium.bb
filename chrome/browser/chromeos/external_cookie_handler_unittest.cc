// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_cookie_handler.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_options.h"
#include "net/base/cookie_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test ExternalCookieHandlerTest;

static const std::string cookie1 = "coookie1\n";
static const std::string cookie2 = "coookie2\n";
static const std::string cookie3 = "coookie3";

class MockCookieStore : public net::CookieStore {
 public:
  MockCookieStore() : expected_url_(ExternalCookieHandler::kGoogleAccountsUrl) {
    cookies_.insert(cookie1);
    cookies_.insert(cookie2);
    cookies_.insert(cookie3);
  }
  virtual ~MockCookieStore() {}

  virtual bool SetCookie(const GURL& url, const std::string& cookie_line) {
    EXPECT_TRUE(false);
    return true;
  }
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const net::CookieOptions& options) {
    EXPECT_FALSE(options.exclude_httponly());
    EXPECT_EQ(expected_url_, url);
    std::set<std::string>::iterator it;
    it = cookies_.find(cookie_line);
    bool has_cookie = cookies_.end() != it;
    if (has_cookie)
      cookies_.erase(it);
    return has_cookie;
  }

  virtual bool SetCookieWithCreationTime(const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time) {
    EXPECT_TRUE(false);
    return true;
  }
  virtual bool SetCookieWithCreationTimeWithOptions(
                                         const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time,
                                         const net::CookieOptions& options) {
    EXPECT_TRUE(false);
    return true;
  }

  virtual void SetCookies(const GURL& url,
                          const std::vector<std::string>& cookies) {
    EXPECT_TRUE(false);
  }
  virtual void SetCookiesWithOptions(const GURL& url,
                                     const std::vector<std::string>& cookies,
                                     const net::CookieOptions& options) {
    EXPECT_TRUE(false);
  }

  virtual std::string GetCookies(const GURL& url) {
    EXPECT_TRUE(false);
    return std::string();
  }
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const net::CookieOptions& options) {
    EXPECT_TRUE(false);
    return std::string();
  }

 private:
  std::set<std::string> cookies_;
  const GURL expected_url_;

  DISALLOW_EVIL_CONSTRUCTORS(MockCookieStore);
};

TEST_F(ExternalCookieHandlerTest, MockCookieStoreSanityTest) {
  GURL url(ExternalCookieHandler::kGoogleAccountsUrl);
  // Need to use a scoped_refptr here because net::CookieStore extends
  // base::RefCountedThreadSafe<> in base/ref_counted.h.
  scoped_refptr<MockCookieStore> cookie_store(new MockCookieStore);
  net::CookieOptions options;
  options.set_include_httponly();
  EXPECT_TRUE(cookie_store->SetCookieWithOptions(url, cookie1, options));
  EXPECT_TRUE(cookie_store->SetCookieWithOptions(url, cookie2, options));
  EXPECT_TRUE(cookie_store->SetCookieWithOptions(url, cookie3, options));
  EXPECT_FALSE(cookie_store->SetCookieWithOptions(url, cookie1, options));
  EXPECT_FALSE(cookie_store->SetCookieWithOptions(url, cookie2, options));
  EXPECT_FALSE(cookie_store->SetCookieWithOptions(url, cookie3, options));
}

class MockReader : public PipeReader {
 public:
  explicit MockReader(const std::vector<std::string>& cookies)
      : data_(cookies) {
  }

  std::string Read(const uint32 bytes_to_read) {
    std::string to_return;
    if (!data_.empty()) {
      to_return = data_.back();
      data_.pop_back();
    }
    return to_return;
  }
 private:
  std::vector<std::string> data_;
};

TEST_F(ExternalCookieHandlerTest, SuccessfulReadTest) {
  GURL url(ExternalCookieHandler::kGoogleAccountsUrl);

  scoped_refptr<MockCookieStore> cookie_store(new MockCookieStore);

  std::vector<std::string> cookies;
  cookies.push_back(cookie3);
  cookies.push_back(cookie2);
  cookies.push_back(cookie1);
  MockReader *reader = new MockReader(cookies);

  ExternalCookieHandler handler(reader);  // takes ownership.
  EXPECT_TRUE(handler.HandleCookies(cookie_store.get()));
}

TEST_F(ExternalCookieHandlerTest, SuccessfulSlowReadTest) {
  GURL url(ExternalCookieHandler::kGoogleAccountsUrl);

  scoped_refptr<MockCookieStore> cookie_store(new MockCookieStore);

  std::vector<std::string> cookies;
  cookies.push_back(cookie3);
  cookies.push_back(cookie2.substr(2));
  cookies.push_back(cookie2.substr(0, 2));
  cookies.push_back(cookie1);
  MockReader *reader = new MockReader(cookies);

  ExternalCookieHandler handler(reader);  // takes ownership.
  EXPECT_TRUE(handler.HandleCookies(cookie_store.get()));
}

}  // namespace chromeos
