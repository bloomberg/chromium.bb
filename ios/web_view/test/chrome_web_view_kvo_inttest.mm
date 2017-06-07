// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/testing/wait_util.h"
#import "ios/web_view/test/chrome_web_view_test.h"
#import "ios/web_view/test/observer.h"
#import "ios/web_view/test/web_view_interaction_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that the KVO compliant properties of CWVWebView correctly report
// changes.
class ChromeWebViewKvoTest : public ios_web_view::ChromeWebViewTest {
 protected:
  ChromeWebViewKvoTest() {
    CWVWebViewConfiguration* configuration =
        [CWVWebViewConfiguration defaultConfiguration];
    web_view_.reset([[CWVWebView alloc]
        initWithFrame:CGRectMake(0.0, 0.0, 100.0, 100.0)
        configuration:configuration]);
  }

  // Web View used to listen for expected KVO property changes.
  base::scoped_nsobject<CWVWebView> web_view_;
};

namespace ios_web_view {

// Tests that CWVWebView correctly reports |canGoBack| and |canGoForward| state.
TEST_F(ChromeWebViewKvoTest, CanGoBackForward) {
  Observer* back_observer = [[Observer alloc] init];
  [back_observer setObservedObject:web_view_ keyPath:@"canGoBack"];

  Observer* forward_observer = [[Observer alloc] init];
  [forward_observer setObservedObject:web_view_ keyPath:@"canGoForward"];

  ASSERT_FALSE(back_observer.lastValue);
  ASSERT_FALSE(forward_observer.lastValue);

  // Define pages in reverse order so the links can reference the "next" page.
  GURL page_3_url = GetUrlForPageWithTitle("Page 3");

  std::string page_2_html =
      "<a id='link_2' href='" + page_3_url.spec() + "'>Link 2</a>";
  GURL page_2_url = GetUrlForPageWithHtmlBody(page_2_html);

  std::string page_1_html =
      "<a id='link_1' href='" + page_2_url.spec() + "'>Link 1</a>";
  GURL page_1_url = GetUrlForPageWithHtmlBody(page_1_html);

  LoadUrl(web_view_, net::NSURLWithGURL(page_1_url));
  // Loading initial URL should not affect back/forward navigation state.
  EXPECT_FALSE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapChromeWebViewElementWithId(web_view_, @"link_1"));
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate back to page 1.
  [web_view_ goBack];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_FALSE([back_observer.lastValue boolValue]);
  EXPECT_TRUE([forward_observer.lastValue boolValue]);

  // Navigate forward to page 2.
  [web_view_ goForward];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate to page 3.
  EXPECT_TRUE(test::TapChromeWebViewElementWithId(web_view_, @"link_2"));
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_FALSE([forward_observer.lastValue boolValue]);

  // Navigate back to page 2.
  [web_view_ goBack];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([back_observer.lastValue boolValue]);
  EXPECT_TRUE([forward_observer.lastValue boolValue]);
}

// Tests that CWVWebView correctly reports current |title|.
TEST_F(ChromeWebViewKvoTest, Title) {
  Observer* observer = [[Observer alloc] init];
  [observer setObservedObject:web_view_ keyPath:@"title"];

  NSString* page_2_title = @"Page 2";
  GURL page_2_url =
      GetUrlForPageWithTitle(base::SysNSStringToUTF8(page_2_title));

  NSString* page_1_title = @"Page 1";
  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2_url.spec().c_str());
  GURL page_1_url = GetUrlForPageWithTitleAndBody(
      base::SysNSStringToUTF8(page_1_title), page_1_html);

  LoadUrl(web_view_, net::NSURLWithGURL(page_1_url));
  EXPECT_NSEQ(page_1_title, observer.lastValue);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapChromeWebViewElementWithId(web_view_, @"link_1"));
  WaitForPageLoadCompletion(web_view_);
  EXPECT_NSEQ(page_2_title, observer.lastValue);

  // Navigate back to page 1.
  [web_view_ goBack];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_NSEQ(page_1_title, observer.lastValue);
}

// Tests that CWVWebView correctly reports |isLoading| value.
TEST_F(ChromeWebViewKvoTest, Loading) {
  Observer* observer = [[Observer alloc] init];
  [observer setObservedObject:web_view_ keyPath:@"loading"];

  GURL page_2_url = GetUrlForPageWithTitle("Page 2");

  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2_url.spec().c_str());
  GURL page_1_url = GetUrlForPageWithTitleAndBody("Page 1", page_1_html);

  LoadUrl(web_view_, net::NSURLWithGURL(page_1_url));
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapChromeWebViewElementWithId(web_view_, @"link_1"));
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);

  // Navigate back to page 1.
  [web_view_ goBack];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_TRUE([observer.previousValue boolValue]);
  EXPECT_FALSE([observer.lastValue boolValue]);
}

// Tests that CWVWebView correctly reports |visibleURL| and |lastCommittedURL|.
TEST_F(ChromeWebViewKvoTest, URLs) {
  Observer* last_committed_url_observer = [[Observer alloc] init];
  [last_committed_url_observer setObservedObject:web_view_
                                         keyPath:@"lastCommittedURL"];

  Observer* visible_url_observer = [[Observer alloc] init];
  [visible_url_observer setObservedObject:web_view_ keyPath:@"visibleURL"];

  GURL page_2 = GetUrlForPageWithTitle("Page 2");
  NSURL* page_2_url = net::NSURLWithGURL(page_2);

  std::string page_1_html = base::StringPrintf(
      "<a id='link_1' href='%s'>Link 1</a>", page_2.spec().c_str());
  NSURL* page_1_url =
      net::NSURLWithGURL(GetUrlForPageWithTitleAndBody("Page 1", page_1_html));

  [web_view_ loadRequest:[NSURLRequest requestWithURL:page_1_url]];

  // |visibleURL| will update immediately
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);

  WaitForPageLoadCompletion(web_view_);
  EXPECT_NSEQ(page_1_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);

  // Navigate to page 2.
  EXPECT_TRUE(test::TapChromeWebViewElementWithId(web_view_, @"link_1"));
  WaitForPageLoadCompletion(web_view_);
  EXPECT_NSEQ(page_2_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_2_url, visible_url_observer.lastValue);

  // Navigate back to page 1.
  [web_view_ goBack];
  WaitForPageLoadCompletion(web_view_);
  EXPECT_NSEQ(page_1_url, last_committed_url_observer.lastValue);
  EXPECT_NSEQ(page_1_url, visible_url_observer.lastValue);
}

}  // namespace ios_web_view
