// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_int_test.h"

#import "base/ios/block_types.h"
#include "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_view_creation_util.h"

namespace web {

#pragma mark - IntTestWebStateObserver

// WebStateObserver class that is used to track when page loads finish.
class IntTestWebStateObserver : public web::WebStateObserver {
 public:
  IntTestWebStateObserver(web::WebState* web_state)
      : web::WebStateObserver(web_state), page_loaded_(false) {}

  // Instructs the observer to listen for page loads for |url|.
  void ExpectPageLoad(const GURL& url) {
    expected_url_ = url;
    page_loaded_ = false;
  }

  // Whether |expected_url_| has been loaded successfully.
  bool IsExpectedPageLoaded() { return page_loaded_; }

  // WebStateObserver methods:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override {
    ASSERT_EQ(load_completion_status == web::PageLoadCompletionStatus::SUCCESS,
              expected_url_.is_valid());
    page_loaded_ = true;
  }

 private:
  GURL expected_url_;
  bool page_loaded_;
};

#pragma mark - WebIntTest

WebIntTest::WebIntTest() {}
WebIntTest::~WebIntTest() {}

void WebIntTest::SetUp() {
  WebTest::SetUp();

  // Start the http server.
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  ASSERT_FALSE(server.IsRunning());
  server.StartOrDie();

  // Remove any previously existing WKWebView data.
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  // Create the WebState.
  web::WebState::CreateParams web_state_create_params(GetBrowserState());
  web_state_ = web::WebState::Create(web_state_create_params);

  // Resize the webview so that pages can be properly rendered.
  web_state()->GetView().frame =
      [UIApplication sharedApplication].keyWindow.bounds;

  // Enable web usage for the WebState.
  web_state()->SetWebUsageEnabled(true);

  web_state()->SetDelegate(&web_state_delegate_);
}

void WebIntTest::TearDown() {
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.Stop();
  EXPECT_FALSE(server.IsRunning());

  WebTest::TearDown();
}

id WebIntTest::ExecuteJavaScript(NSString* script) {
  return web::ExecuteJavaScript(web_state()->GetJSInjectionReceiver(), script);
}

void WebIntTest::ExecuteBlockAndWaitForLoad(const GURL& url,
                                            ProceduralBlock block) {
  DCHECK(block);
  observer_ = base::MakeUnique<IntTestWebStateObserver>(web_state());
  observer_->ExpectPageLoad(url);
  block();
  base::test::ios::WaitUntilCondition(^bool {
    return observer_->IsExpectedPageLoaded();
  });
}

void WebIntTest::LoadUrl(const GURL& url) {
  ExecuteBlockAndWaitForLoad(url, ^{
    web::NavigationManager::WebLoadParams params(url);
    navigation_manager()->LoadURLWithParams(params);
  });
}

void WebIntTest::RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                            NSSet* websiteDataTypes) {
  __block bool data_removed = false;

  ProceduralBlock remove_data = ^{
    [data_store removeDataOfTypes:websiteDataTypes
                    modifiedSince:[NSDate distantPast]
                completionHandler:^{
                  data_removed = true;
                }];
  };

  if ([websiteDataTypes containsObject:WKWebsiteDataTypeCookies]) {
    // TODO(crbug.com/554225): This approach of creating a WKWebView and
    // executing JS to clear cookies is a workaround for
    // https://bugs.webkit.org/show_bug.cgi?id=149078.
    // Remove this, when that bug is fixed. The |marker_web_view| will be
    // released when cookies have been cleared.
    WKWebView* marker_web_view =
        web::BuildWKWebView(CGRectZero, GetBrowserState());
    [marker_web_view evaluateJavaScript:@""
                      completionHandler:^(id, NSError*) {
                        [marker_web_view self];
                        remove_data();
                      }];
  } else {
    remove_data();
  }

  base::test::ios::WaitUntilCondition(^bool {
    return data_removed;
  });
}

NSInteger WebIntTest::GetIndexOfNavigationItem(
    const web::NavigationItem* item) {
  for (NSInteger i = 0; i < navigation_manager()->GetItemCount(); ++i) {
    if (navigation_manager()->GetItemAtIndex(i) == item)
      return i;
  }
  return NSNotFound;
}

}  // namespace web
