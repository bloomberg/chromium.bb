// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/test_chrome_web_ui_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrictMock;

// Dummy URL location for us to override.
const char kDummyURL[] = "chrome://DummyURL/";

// Returns a new WebUI object for the TabContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return new ChromeWebUI(arg0);
}

// Mock the TestChromeWebUIFactory::WebUIProvider to prove that we are called as
// expected.
class MockWebUIProvider : public TestChromeWebUIFactory::WebUIProvider {
 public:
  MockWebUIProvider();
  ~MockWebUIProvider();

  MOCK_METHOD2(NewWebUI, WebUI*(TabContents* tab_contents, const GURL& url));
};

class WebUIAssertionsTest : public WebUIBrowserTest {
 public:
  WebUIAssertionsTest();

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    TestChromeWebUIFactory::AddFactoryOverride(
        GURL(kDummyURL).host(), &mock_provider_);
    EXPECT_CALL(mock_provider_, NewWebUI(_, Eq(GURL(kDummyURL))))
        .WillOnce(ReturnNewWebUI());
  }
  virtual void CleanUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::CleanUpOnMainThread();
    TestChromeWebUIFactory::RemoveFactoryOverride(
        GURL(kDummyURL).host());
  }

  StrictMock<MockWebUIProvider> mock_provider_;

 protected:
  ~WebUIAssertionsTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUIAssertionsTest);
};
