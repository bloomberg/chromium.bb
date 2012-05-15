// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_cookie_helper.h"


#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class BrowsingDataCookieHelperTest : public testing::Test {
 public:
  void SetUpOnIOThread(base::WaitableEvent* io_setup_complete) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // This is a workaround for a bug in the TestingProfile.
    // The URLRequestContext will be created by GetCookieMonster on the UI
    // thread, if it does not already exist. But it must be created on the IO
    // thread or else it will DCHECK upon destruction.
    // Force it to be created here.
    testing_profile_->CreateRequestContext();
    testing_profile_->GetRequestContext()->GetURLRequestContext();
    io_setup_complete->Signal();
  }

  virtual void SetUp() {
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &message_loop_));
    // Note: we're starting a real IO thread because parts of the
    // BrowsingDataCookieHelper expect to run on that thread.
    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());
    testing_profile_.reset(new TestingProfile());
    base::WaitableEvent io_setup_complete(true, false);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataCookieHelperTest::SetUpOnIOThread,
                   base::Unretained(this), &io_setup_complete));
    io_setup_complete.Wait();
  }

  virtual void TearDown() {
    // This must be reset before the IO thread stops, because the
    // URLRequestContextGetter forces its own deletion to occur on that thread.
    testing_profile_->ResetRequestContext();
    io_thread_.reset();
    ui_thread_.reset();
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
    MessageLoop::current()->Quit();
  }

  void DeleteCallback(const net::CookieList& cookies) {
    ASSERT_EQ(1UL, cookies.size());
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("www.gmail.google.com", it->Domain());
    EXPECT_EQ("B", it->Name());

    ASSERT_TRUE(++it == cookies.end());
    MessageLoop::current()->Quit();
  }

  void CannedUniqueCallback(const net::CookieList& cookies) {
    ASSERT_EQ(1UL, cookies.size());
    cookie_list_ = cookies;
    net::CookieList::const_iterator it = cookies.begin();

    ASSERT_TRUE(it != cookies.end());
    EXPECT_EQ("http://www.google.com/", it->Source());
    EXPECT_EQ("A", it->Name());

    ASSERT_TRUE(++it == cookies.end());
  }

  void CannedDifferentFramesCallback(const net::CookieList& cookie_list) {
    ASSERT_EQ(3U, cookie_list.size());
  }

 protected:
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  scoped_ptr<TestingProfile> testing_profile_;

  net::CookieList cookie_list_;
};

TEST_F(BrowsingDataCookieHelperTest, FetchData) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(testing_profile_.get()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));

  // Blocks until BrowsingDataCookieHelperTest::FetchCallback is notified.
  MessageLoop::current()->Run();
}

TEST_F(BrowsingDataCookieHelperTest, DeleteCookie) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(testing_profile_.get()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));

  // Blocks until BrowsingDataCookieHelperTest::FetchCallback is notified.
  MessageLoop::current()->Run();

  net::CookieMonster::CanonicalCookie cookie = cookie_list_[0];
  cookie_helper->DeleteCookie(cookie);

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DeleteCallback,
                 base::Unretained(this)));
  MessageLoop::current()->Run();
}

TEST_F(BrowsingDataCookieHelperTest, CannedUnique) {
  const GURL origin("http://www.google.com");
  net::CookieList cookie;

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(testing_profile_.get()));

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

TEST_F(BrowsingDataCookieHelperTest, CannedEmpty) {
  const GURL url_google("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(testing_profile_.get()));

  ASSERT_TRUE(helper->empty());
  helper->AddChangedCookie(url_google, url_google, "a=1",
                          net::CookieOptions());
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  net::CookieList cookies;
  net::CookieMonster::ParsedCookie pc("a=1");
  scoped_ptr<net::CookieMonster::CanonicalCookie> cookie(
      new net::CookieMonster::CanonicalCookie(url_google, pc));
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
      new CannedBrowsingDataCookieHelper(testing_profile_.get()));

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

}  // namespace
