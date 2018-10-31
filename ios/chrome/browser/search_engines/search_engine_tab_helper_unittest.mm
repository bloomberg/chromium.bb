// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"

#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace {
const char kOpenSearchXmlFilePath[] =
    "/ios/testing/data/http_server_files/opensearch.xml";

// A BrowserStateKeyedServiceFactory::TestingFactory that creates a testing
// TemplateURLService. The created TemplateURLService may contain some default
// TemplateURLs.
std::unique_ptr<KeyedService> CreateTestingTemplateURLService(
    web::BrowserState*) {
  return std::make_unique<TemplateURLService>(/*initializers=*/nullptr,
                                              /*count=*/0);
}
}

// Test fixture for SearchEngineTabHelper class.
class SearchEngineTabHelperTest : public ChromeWebTest {
 protected:
  SearchEngineTabHelperTest()
      : ChromeWebTest(web::TestWebThreadBundle::Options::IO_MAINLOOP) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    SearchEngineTabHelper::CreateForWebState(web_state());
    server_.ServeFilesFromSourceDirectory(".");
    ASSERT_TRUE(server_.Start());
    // Set the TestingFactory of BrowserState of ios::TemplateURLServiceFactory
    // to provide a testing TemplateURLService. Notice that we should not use
    // GetBrowserState() because it may be overriden to return incognito
    // BrowserState but TemplateURLService is always created on regular
    // BrowserState.
    ios::TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_->GetOriginalChromeBrowserState(),
        base::BindRepeating(&CreateTestingTemplateURLService));
    template_url_service()->Load();
  }

  // Returns the testing TemplateURLService.
  TemplateURLService* template_url_service() {
    ios::ChromeBrowserState* browser_state =
        ios::ChromeBrowserState::FromBrowserState(GetBrowserState());
    return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  }

  // Sends a message that a OSDD <link> is found in page, with |page_url| and
  // |osdd_url| as message content.
  bool SendMessageOfOpenSearch(const GURL& page_url, const GURL& osdd_url) {
    id result = ExecuteJavaScript([NSString
        stringWithFormat:@"__gCrWeb.message.invokeOnHost({'command': "
                         @"'searchEngine.openSearch', 'pageUrl' : '%s', "
                         @"'osddUrl': '%s'}); true;",
                         page_url.spec().c_str(), osdd_url.spec().c_str()]);
    return [result isEqual:@YES];
  }

  net::EmbeddedTestServer server_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelperTest);
};

// Tests that SearchEngineTabHelper can add TemplateURL to TemplateURLService
// when a OSDD <link> is found in web page.
TEST_F(SearchEngineTabHelperTest, AddTemplateURLByOpenSearch) {
  GURL page_url("https://chrooome.com");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(@"<html></html>", page_url);
  ASSERT_TRUE(SendMessageOfOpenSearch(page_url, osdd_url));

  // Wait for TemplateURL added to TemplateURLService.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));

  // TemplateURLService doesn't guarantee that newly added TemplateURL is
  // appended, so we should check in a general way that only one TemplateURL is
  // added and others remain untouched.
  TemplateURL* new_url = nullptr;
  for (TemplateURL* url : template_url_service()->GetTemplateURLs()) {
    if (std::find(old_urls.begin(), old_urls.end(), url) == old_urls.end()) {
      ASSERT_FALSE(new_url);
      new_url = url;
    }
  }
  ASSERT_TRUE(new_url);
  EXPECT_EQ(base::UTF8ToUTF16("chrooome.com"), new_url->data().keyword());
  EXPECT_EQ(base::UTF8ToUTF16("Chrooome"), new_url->data().short_name());
  EXPECT_EQ(
      "https://chrooome.com/index.php?title=chrooome&search={searchTerms}",
      new_url->data().url());
}

// Test fixture for SearchEngineTabHelper class in incognito mode.
class SearchEngineTabHelperIncognitoTest : public SearchEngineTabHelperTest {
 protected:
  SearchEngineTabHelperIncognitoTest() {}

  // Overrides WebTest::GetBrowserState.
  web::BrowserState* GetBrowserState() override {
    return chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  }

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelperIncognitoTest);
};

// Tests that SearchEngineTabHelper doesn't add TemplateURL to
// TemplateURLService when a OSDD <link> is found in web page under incognito
// mode.
TEST_F(SearchEngineTabHelperIncognitoTest,
       NotAddTemplateURLByOpenSearchUnderIncognito) {
  GURL page_url("https://chrooome.com");
  GURL osdd_url = server_.GetURL(kOpenSearchXmlFilePath);

  // Record the original TemplateURLs in TemplateURLService.
  std::vector<TemplateURL*> old_urls =
      template_url_service()->GetTemplateURLs();

  // Load an empty page, and send a message of openSearchUrl from Js.
  LoadHtml(@"<html></html>", page_url);
  ASSERT_TRUE(SendMessageOfOpenSearch(page_url, osdd_url));

  // No new TemplateURL should be added to TemplateURLService, wait for timeout.
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return template_url_service()->GetTemplateURLs().size() ==
           old_urls.size() + 1;
  }));
  EXPECT_EQ(old_urls, template_url_service()->GetTemplateURLs());
}
