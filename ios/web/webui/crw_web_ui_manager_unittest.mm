// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/web_state/web_state_impl.h"
#import "ios/web/webui/crw_web_ui_page_builder.h"
#include "ios/web/webui/url_fetcher_block_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {

// Path for test favicon file.
const char kFaviconPath[] = "ios/web/test/data/testfavicon.png";
// URL for mock WebUI page.
const char kTestChromeUrl[] = "chrome://test";
// URL for mock favicon.
const char kFaviconUrl[] = "chrome://favicon";
// HTML for mock WebUI page.
NSString* kHtml = @"<html>Hello World</html>";

// Mock of WebStateImpl to check that LoadHtml and ExecuteJavaScriptAsync are
// called as expected.
class MockWebStateImpl : public WebStateImpl {
 public:
  MockWebStateImpl(BrowserState* browser_state) : WebStateImpl(browser_state) {}
  MOCK_METHOD2(LoadWebUIHtml,
               void(const base::string16& html, const GURL& url));
  MOCK_METHOD1(ExecuteJavaScriptAsync, void(const base::string16& javascript));
};

// Mock of URLFetcherBlockAdapter to provide mock resources.
class MockURLFetcherBlockAdapter : public URLFetcherBlockAdapter {
 public:
  MockURLFetcherBlockAdapter(
      const GURL& url,
      net::URLRequestContextGetter* request_context,
      URLFetcherBlockAdapterCompletion completion_handler)
      : URLFetcherBlockAdapter(url, request_context, completion_handler),
        url_(url),
        completion_handler_(completion_handler) {}

  void Start() override {
    if (url_.spec() == kTestChromeUrl) {
      completion_handler_.get()([kHtml dataUsingEncoding:NSUTF8StringEncoding],
                                this);
    } else if (url_.spec() == kFaviconUrl) {
      base::FilePath favicon_path;
      ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &favicon_path));
      favicon_path = favicon_path.AppendASCII(kFaviconPath);
      completion_handler_.get()(
          [NSData dataWithContentsOfFile:base::SysUTF8ToNSString(
                                             favicon_path.value())],
          this);
    } else {
      NOTREACHED();
    }
  }

 private:
  // The URL to fetch.
  const GURL url_;
  // Callback for resource load.
  base::mac::ScopedBlock<URLFetcherBlockAdapterCompletion> completion_handler_;
};

class AppSpecificTestWebClient : public TestWebClient {
public:
  bool IsAppSpecificURL(const GURL& url) const override {
    return url.SchemeIs("chrome");
  }
};

}  // namespace web

// Subclass of CRWWebUIManager for testing.
@interface CRWTestWebUIManager : CRWWebUIManager
// Use mock URLFetcherBlockAdapter.
- (scoped_ptr<web::URLFetcherBlockAdapter>)
        fetcherForURL:(const GURL&)URL
    completionHandler:(web::URLFetcherBlockAdapterCompletion)handler;
@end

@implementation CRWTestWebUIManager
- (scoped_ptr<web::URLFetcherBlockAdapter>)
        fetcherForURL:(const GURL&)URL
    completionHandler:(web::URLFetcherBlockAdapterCompletion)handler {
  return scoped_ptr<web::URLFetcherBlockAdapter>(
      new web::MockURLFetcherBlockAdapter(URL, nil, handler));
}
@end

namespace web {

// Test fixture for testing CRWWebUIManager
class CRWWebUIManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    test_web_client_.reset(new web::AppSpecificTestWebClient());
    web::SetWebClient(test_web_client_.get());
    test_browser_state_.reset(new TestBrowserState());
    web_state_impl_.reset(new MockWebStateImpl(test_browser_state_.get()));
    web_ui_manager_.reset(
        [[CRWTestWebUIManager alloc] initWithWebState:web_state_impl_.get()]);
  }

  // TestBrowserState for creation of WebStateImpl.
  scoped_ptr<TestBrowserState> test_browser_state_;
  // MockWebStateImpl for detection of LoadHtml and EvaluateJavaScriptAync
  // calls.
  scoped_ptr<MockWebStateImpl> web_state_impl_;
  // WebUIManager for testing.
  base::scoped_nsobject<CRWTestWebUIManager> web_ui_manager_;
  // The WebClient used in tests.
  scoped_ptr<AppSpecificTestWebClient> test_web_client_;
};

// Tests that CRWWebUIManager observes provisional navigation and invokes an
// HTML load in web state.
TEST_F(CRWWebUIManagerTest, LoadWebUI) {
  base::string16 html(base::SysNSStringToUTF16(kHtml));
  GURL url(kTestChromeUrl);
  EXPECT_CALL(*web_state_impl_, LoadWebUIHtml(html, url));
  web_state_impl_->OnProvisionalNavigationStarted(url);
}

// Tests that CRWWebUIManager responds to OnScriptCommandReceieved call and runs
// JavaScript to set favicon background.
TEST_F(CRWWebUIManagerTest, HandleFaviconRequest) {
  GURL test_url(kTestChromeUrl);
  std::string favicon_url_string(kFaviconUrl);

  // Create mock JavaScript message to request favicon.
  base::ListValue* arguments(new base::ListValue());
  arguments->AppendString(favicon_url_string);
  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue());
  message->SetString("message", "webui.requestFavicon");
  message->Set("arguments", arguments);

  // Create expected JavaScript to call.
  base::FilePath favicon_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &favicon_path));
  favicon_path = favicon_path.AppendASCII(kFaviconPath);
  NSData* expected_data = [NSData
      dataWithContentsOfFile:base::SysUTF8ToNSString(favicon_path.value())];
  base::string16 expected_javascript = base::SysNSStringToUTF16([NSString
      stringWithFormat:
          @"chrome.setFaviconBackground('%s', 'data:image/png;base64,%@');",
          favicon_url_string.c_str(),
          [expected_data base64EncodedStringWithOptions:0]]);

  EXPECT_CALL(*web_state_impl_, ExecuteJavaScriptAsync(expected_javascript));
  web_state_impl_->OnScriptCommandReceived("webui.requestFavicon", *message,
                                           test_url, false);
}
}  // namespace web
