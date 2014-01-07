// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/parsed_cookie.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowsingDataCookieHelperTest : public testing::Test {
 public:
  BrowsingDataCookieHelperTest()
      : testing_profile_(new TestingProfile()) {
  }

  void CreateCookiesForTest() {
    scoped_refptr<net::CookieMonster> cookie_monster =
        testing_profile_->GetCookieMonster();
    cookie_monster->SetCookieWithOptionsAsync(
        GURL("http://www.google.com"), "A=1", net::CookieOptions(),
        net::CookieMonster::SetCookiesCallback());
    cookie_monster->SetCookieWithOptionsAsync(
        GURL("http://www.gmail.google.com"), "B=1", net::CookieOptions(),
        net::CookieMonster::SetCookiesCallback());
  }

  void CreateCookiesForDomainCookieTest() {
    scoped_refptr<net::CookieMonster> cookie_monster =
        testing_profile_->GetCookieMonster();
    cookie_monster->SetCookieWithOptionsAsync(
        GURL("http://www.google.com"), "A=1", net::CookieOptions(),
        net::CookieMonster::SetCookiesCallback());
    cookie_monster->SetCookieWithOptionsAsync(
        GURL("http://www.google.com"), "A=2; Domain=.www.google.com ",
        net::CookieOptions(), net::CookieMonster::SetCookiesCallback());
  }

  void FetchCallback(const net::CookieList& cookies) {
    ASSERT_EQ(2UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    // Correct because fetching cookies will get a sorted cookie list.
    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("www.google.com", it->Domain());
    EXPECT_EQ("A", it->Name());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("www.gmail.google.com", it->Domain());
    EXPECT_EQ("B", it->Name());

    ASSERT_TRUE(++it == cookies.end());
  }

  void DomainCookieCallback(const net::CookieList& cookies) {
    ASSERT_EQ(2UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    // Correct because fetching cookies will get a sorted cookie list.
    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("www.google.com", it->Domain());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("1", it->Value());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ(".www.google.com", it->Domain());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("2", it->Value());

    ASSERT_TRUE(++it == cookies.end());
  }

  void DeleteCallback(const net::CookieList& cookies) {
    ASSERT_EQ(1UL, cookies.size());
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("www.gmail.google.com", it->Domain());
    EXPECT_EQ("B", it->Name());

    ASSERT_TRUE(++it == cookies.end());
  }

  void CannedUniqueCallback(const net::CookieList& cookies) {
    EXPECT_EQ(1UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("www.google.com", it->Domain());
    EXPECT_EQ("/", it->Path());
    EXPECT_EQ("A", it->Name());

    ASSERT_TRUE(++it == cookies.end());
  }

  void CannedReplaceCookieCallback(const net::CookieList& cookies) {
    EXPECT_EQ(5UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("www.google.com", it->Domain());
    EXPECT_EQ("/", it->Path());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("2", it->Value());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("www.google.com", it->Domain());
    EXPECT_EQ("/example/0", it->Path());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("4", it->Value());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ(".google.com", it->Domain());
    EXPECT_EQ("/", it->Path());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("6", it->Value());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ(".google.com", it->Domain());
    EXPECT_EQ("/example/1", it->Path());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("8", it->Value());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ(".www.google.com", it->Domain());
    EXPECT_EQ("/", it->Path());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("10", it->Value());

    ASSERT_TRUE(++it == cookies.end());
  }

  void CannedDomainCookieCallback(const net::CookieList& cookies) {
    ASSERT_EQ(2UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ("www.google.com", it->Domain());

    ASSERT_TRUE(++it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("A", it->Name());
    EXPECT_EQ(".www.google.com", it->Domain());

    ASSERT_TRUE(++it == cookies.end());
  }

  void CannedDifferentFramesCallback(const net::CookieList& cookie_list) {
    ASSERT_EQ(3U, cookie_list.size());
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> testing_profile_;

  net::CookieList cookie_list_;
};

TEST_F(BrowsingDataCookieHelperTest, FetchData) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(testing_profile_->GetRequestContext()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, DomainCookie) {
  CreateCookiesForDomainCookieTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(testing_profile_->GetRequestContext()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DomainCookieCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, DeleteCookie) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(testing_profile_->GetRequestContext()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  net::CanonicalCookie cookie = cookie_list_[0];
  cookie_helper->DeleteCookie(cookie);

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DeleteCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, CannedDeleteCookie) {
  CreateCookiesForTest();
  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());

  const GURL origin1("http://www.google.com");
  const GURL origin2("http://www.gmail.google.com");
  helper->AddChangedCookie(origin1, origin1, "A=1", net::CookieOptions());
  helper->AddChangedCookie(origin2, origin2, "B=1", net::CookieOptions());

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, helper->GetCookieCount());

  helper->DeleteCookie(cookie_list_[0]);

  EXPECT_EQ(1u, helper->GetCookieCount());
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DeleteCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, CannedDomainCookie) {
  const GURL origin("http://www.google.com");
  net::CookieList cookie;

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(origin, origin, "A=1", net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=1; Domain=.www.google.com",
                           net::CookieOptions());
  // Try adding invalid cookies that will be ignored.
  helper->AddChangedCookie(origin, origin, std::string(), net::CookieOptions());
  helper->AddChangedCookie(origin,
                           origin,
                           "C=bad guy; Domain=wrongdomain.com",
                           net::CookieOptions());

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDomainCookieCallback,
                 base::Unretained(this)));
  cookie = cookie_list_;

  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDomainCookieCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedUnique) {
  const GURL origin("http://www.google.com");
  net::CookieList cookie;

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(origin, origin, "A=1", net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=1", net::CookieOptions());
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedUniqueCallback,
                 base::Unretained(this)));

  cookie = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie);
  helper->AddReadCookies(origin, origin, cookie);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedUniqueCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedReplaceCookie) {
  const GURL origin("http://www.google.com");
  net::CookieList cookie;

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(origin, origin, "A=1", net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=2", net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=3; Path=/example/0",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=4; Path=/example/0",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=5; Domain=google.com",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin, "A=6; Domain=google.com",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin,
                           "A=7; Domain=google.com; Path=/example/1",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin,
                           "A=8; Domain=google.com; Path=/example/1",
                           net::CookieOptions());

  helper->AddChangedCookie(origin, origin,
                          "A=9; Domain=www.google.com",
                           net::CookieOptions());
  helper->AddChangedCookie(origin, origin,
                           "A=10; Domain=www.google.com",
                           net::CookieOptions());

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedReplaceCookieCallback,
                 base::Unretained(this)));

  cookie = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie);
  helper->AddReadCookies(origin, origin, cookie);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedReplaceCookieCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedEmpty) {
  const GURL url_google("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(url_google, url_google, "a=1",
                          net::CookieOptions());
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  net::CookieList cookies;
  net::ParsedCookie pc("a=1");
  scoped_ptr<net::CanonicalCookie> cookie(
      new net::CanonicalCookie(url_google, pc));
  cookies.push_back(*cookie);

  helper->AddReadCookies(url_google, url_google, cookies);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(BrowsingDataCookieHelperTest, CannedDifferentFrames) {
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  GURL request_url("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(frame1_url, request_url, "a=1",
                           net::CookieOptions());
  helper->AddChangedCookie(frame1_url, request_url, "b=1",
                           net::CookieOptions());
  helper->AddChangedCookie(frame2_url, request_url, "c=1",
                           net::CookieOptions());

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDifferentFramesCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedGetCookieCount) {
  // The URL in the omnibox is a frame URL. This is not necessarily the request
  // URL, since websites usually include other resources.
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  // The request URL used for all cookies that are added to the |helper|.
  GURL request1_url("http://static.google.com/foo/res1.html");
  GURL request2_url("http://static.google.com/bar/res2.html");
  std::string cookie_domain(".www.google.com");
  std::string cookie_pair1("A=1");
  std::string cookie_pair2("B=1");
  // The cookie pair used for adding a cookie that overrides the cookie created
  // with |cookie_pair1|. The cookie-name of |cookie_pair3| must match the
  // cookie-name of |cookie-pair1|.
  std::string cookie_pair3("A=2");
  // The cookie pair used for adding a non host-only cookie. The cookie-name
  // must match the cookie-name of |cookie_pair1| in order to add a host-only
  // and a non host-only cookie with the same name below.
  std::string cookie_pair4("A=3");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(
          testing_profile_->GetRequestContext()));

  // Add two different cookies (distinguished by the tuple [cookie-name,
  // domain-value, path-value]) for a HTTP request to |frame1_url| and verify
  // that the cookie count is increased to two. The set-cookie-string consists
  // only of the cookie-pair. This means that the host and the default-path of
  // the |request_url| are used as domain-value and path-value for the added
  // cookies.
  EXPECT_EQ(0U, helper->GetCookieCount());
  helper->AddChangedCookie(frame1_url, frame1_url, cookie_pair1,
                           net::CookieOptions());
  EXPECT_EQ(1U, helper->GetCookieCount());
  helper->AddChangedCookie(frame1_url, frame1_url, cookie_pair2,
                           net::CookieOptions());
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Use a different frame URL for adding another cookie that will replace one
  // of the previously added cookies. This could happen during an automatic
  // redirect e.g. |frame1_url| redirects to |frame2_url| and a cookie set by a
  // request to |frame1_url| is updated.
  helper->AddChangedCookie(frame2_url, frame1_url, cookie_pair3,
                           net::CookieOptions());
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Add two more cookies that are set while loading resources. The two cookies
  // below have a differnt path-value since the request URLs have different
  // paths.
  helper->AddChangedCookie(frame2_url, request1_url, cookie_pair3,
                           net::CookieOptions());
  EXPECT_EQ(3U, helper->GetCookieCount());
  helper->AddChangedCookie(frame2_url, request2_url, cookie_pair3,
                           net::CookieOptions());
  EXPECT_EQ(4U, helper->GetCookieCount());

  // Host-only and domain cookies are treated as seperate items. This means that
  // the following two cookie-strings are stored as two separate cookies, even
  // though they have the same name and are send with the same request:
  //   "A=1;
  //   "A=3; Domain=www.google.com"
  // Add a domain cookie and check if it increases the cookie count.
  helper->AddChangedCookie(frame2_url, frame1_url,
                           cookie_pair4 + "; Domain=" + cookie_domain,
                           net::CookieOptions());
  EXPECT_EQ(5U, helper->GetCookieCount());
}

}  // namespace
