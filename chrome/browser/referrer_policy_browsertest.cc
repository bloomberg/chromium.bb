// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/test_server.h"

namespace {

const FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/referrer_policy");

}  // namespace

class ReferrerPolicyTest : public InProcessBrowserTest {
 public:
  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    ui_test_utils::TitleWatcher title_watcher(
        browser()->GetSelectedWebContents(),
        ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(title_watcher.WaitAndGetTitle(), ASCIIToUTF16(expected_title));
  }

  enum ExpectedReferrer {
    ExpectedReferrerIsEmpty,
    ExpectedReferrerIsFull,
    ExpectedReferrerIsOrigin
  };

  // Navigates from a page with a given |referrer_policy| (optionally on https)
  // to a page on http that reports the passed referrer and checks that it
  // matches the |expected_referrer|.
  void RunReferrerTest(const std::string referrer_policy,
                       bool start_on_https,
                       ExpectedReferrer expected_referrer) {
    net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
    ASSERT_TRUE(test_server.Start());
    net::TestServer ssl_test_server(net::TestServer::TYPE_HTTPS,
                                    FilePath(kDocRoot));
    ASSERT_TRUE(ssl_test_server.Start());

    GURL start_url = test_server.GetURL(
        std::string("files/referrer-policy-start.html?") + referrer_policy +
        "&port=" + base::IntToString(test_server.host_port_pair().port()));
    if (start_on_https) {
      start_url = ssl_test_server.GetURL(
          std::string("files/referrer-policy-start.html?") + referrer_policy +
          "&port=" + base::IntToString(test_server.host_port_pair().port()));
    }

    std::string referrer;
    switch (expected_referrer) {
      case ExpectedReferrerIsEmpty:
        referrer = "Referrer is empty";
        break;
      case ExpectedReferrerIsFull:
        referrer = "Referrer is " + start_url.spec();
        break;
      case ExpectedReferrerIsOrigin:
        referrer = "Referrer is " + start_url.GetWithEmptyPath().spec();
        break;
    }

    NavigateToURLAndWaitForTitle(start_url, referrer, 2);
  }
};

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Default) {
  RunReferrerTest("default", false, ExpectedReferrerIsFull);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Always) {
  RunReferrerTest("always", false, ExpectedReferrerIsFull);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Origin) {
  RunReferrerTest("origin", false, ExpectedReferrerIsOrigin);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Never) {
  RunReferrerTest("never", false, ExpectedReferrerIsEmpty);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsDefault) {
  RunReferrerTest("default", true, ExpectedReferrerIsEmpty);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsAlways) {
  RunReferrerTest("always", true, ExpectedReferrerIsFull);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsOrigin) {
  RunReferrerTest("origin", true, ExpectedReferrerIsOrigin);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsNever) {
  RunReferrerTest("never", true, ExpectedReferrerIsEmpty);
}

IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Redirect) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  net::TestServer ssl_test_server(net::TestServer::TYPE_HTTPS,
                                  FilePath(kDocRoot));
  ASSERT_TRUE(ssl_test_server.Start());
  GURL start_url = test_server.GetURL(
      std::string("files/referrer-policy-redirect.html?") +
      "ssl_port=" + base::IntToString(ssl_test_server.host_port_pair().port()) +
      "&port=" + base::IntToString(test_server.host_port_pair().port()));
  std::string referrer = "Referrer is " + start_url.GetWithEmptyPath().spec();
  NavigateToURLAndWaitForTitle(start_url, referrer, 2);
}
