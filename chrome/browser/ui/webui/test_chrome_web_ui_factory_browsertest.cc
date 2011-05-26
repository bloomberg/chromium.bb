// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/test_chrome_web_ui_factory.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrictMock;

namespace {

// Returns a new WebUI object for the TabContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return new WebUI(arg0);
}

// Mock the TestChromeWebUIFactory::WebUIProvider to prove that we are called as
// expected.
class MockWebUIProvider : public TestChromeWebUIFactory::WebUIProvider {
 public:
  MOCK_METHOD2(NewWebUI, WebUI*(TabContents* tab_contents, const GURL& url));
};

// Dummy URL location for us to override.
const char kChromeTestChromeWebUIFactory[] =
    "chrome://ChromeTestChromeWebUIFactory/";

// Sets up and tears down the factory override for our url's host. It is
// necessary to do this here, rather than in the test declaration, which is too
// late to catch the possibility of an initial browse to about:blank mistakenly
// going to this handler.
class TestChromeWebUIFactoryTest : public InProcessBrowserTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    TestChromeWebUIFactory::AddFactoryOverride(
        GURL(kChromeTestChromeWebUIFactory).host(), &mock_provider_);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    TestChromeWebUIFactory::RemoveFactoryOverride(
        GURL(kChromeTestChromeWebUIFactory).host());
  }

 protected:
  StrictMock<MockWebUIProvider> mock_provider_;
};

}  // namespace

// Test that browsing to our test url causes us to be called once.
IN_PROC_BROWSER_TEST_F(TestChromeWebUIFactoryTest, TestWebUIProvider) {
  const GURL kChromeTestChromeWebUIFactoryURL(kChromeTestChromeWebUIFactory);
  EXPECT_CALL(mock_provider_, NewWebUI(_, Eq(kChromeTestChromeWebUIFactoryURL)))
      .WillOnce(ReturnNewWebUI());
  ui_test_utils::NavigateToURL(browser(), kChromeTestChromeWebUIFactoryURL);
}
