// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class WKWebViewConfigurationProviderTest : public PlatformTest {
 public:
  WKWebViewConfigurationProviderTest()
      : web_client_(base::WrapUnique(new web::WebClient)) {}

 protected:
  // Returns WKWebViewConfigurationProvider associated with |browser_state_|.
  WKWebViewConfigurationProvider& GetProvider() {
    return GetProvider(&browser_state_);
  }
  // Returns WKWebViewConfigurationProvider for given |browser_state|.
  WKWebViewConfigurationProvider& GetProvider(
      BrowserState* browser_state) const {
    return WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  }
  // BrowserState required for WKWebViewConfigurationProvider creation.
  web::ScopedTestingWebClient web_client_;
  TestBrowserState browser_state_;
};

// Tests that each WKWebViewConfigurationProvider has own, non-nil
// configuration and configurations returned by the same provider will always
// have the same process pool.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationOwnerhip) {
  // Configuration is not nil.
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  ASSERT_TRUE(provider.GetWebViewConfiguration());

  // Same non-nil WKProcessPool for the same provider.
  ASSERT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(provider.GetWebViewConfiguration().processPool,
            provider.GetWebViewConfiguration().processPool);

  // Different WKProcessPools for different providers.
  TestBrowserState other_browser_state;
  WKWebViewConfigurationProvider& other_provider =
      GetProvider(&other_browser_state);
  EXPECT_NE(provider.GetWebViewConfiguration().processPool,
            other_provider.GetWebViewConfiguration().processPool);
}

// Tests Non-OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, NoneOffTheRecordConfiguration) {
  browser_state_.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  EXPECT_TRUE(provider.GetWebViewConfiguration().websiteDataStore.persistent);
}

// Tests OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, OffTheRecordConfiguration) {
  browser_state_.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  ASSERT_TRUE(config);
  EXPECT_FALSE(config.websiteDataStore.persistent);
}

// Tests that internal configuration object can not be changed by clients.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationProtection) {
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  base::scoped_nsobject<WKProcessPool> pool([[config processPool] retain]);
  base::scoped_nsobject<WKPreferences> prefs([[config preferences] retain]);
  base::scoped_nsobject<WKUserContentController> userContentController(
      [[config userContentController] retain]);

  // Change the properties of returned configuration object.
  TestBrowserState other_browser_state;
  WKWebViewConfiguration* other_wk_web_view_configuration =
      GetProvider(&other_browser_state).GetWebViewConfiguration();
  ASSERT_TRUE(other_wk_web_view_configuration);
  config.processPool = other_wk_web_view_configuration.processPool;
  config.preferences = other_wk_web_view_configuration.preferences;
  config.userContentController =
      other_wk_web_view_configuration.userContentController;

  // Make sure that the properties of internal configuration were not changed.
  EXPECT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(pool.get(), provider.GetWebViewConfiguration().processPool);
  EXPECT_TRUE(provider.GetWebViewConfiguration().preferences);
  EXPECT_EQ(prefs.get(), provider.GetWebViewConfiguration().preferences);
  EXPECT_TRUE(provider.GetWebViewConfiguration().userContentController);
  EXPECT_EQ(userContentController.get(),
            provider.GetWebViewConfiguration().userContentController);
}

// Tests that script message router is bound to correct user content controller.
TEST_F(WKWebViewConfigurationProviderTest, ScriptMessageRouter) {
  ASSERT_TRUE(GetProvider().GetWebViewConfiguration().userContentController);
  EXPECT_EQ(GetProvider().GetWebViewConfiguration().userContentController,
            GetProvider().GetScriptMessageRouter().userContentController);
}

// Tests that both configuration and script message router are deallocated after
// |Purge| call.
TEST_F(WKWebViewConfigurationProviderTest, Purge) {
  base::WeakNSObject<id> config;
  base::WeakNSObject<id> router;
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    config.reset(GetProvider().GetWebViewConfiguration());
    router.reset(GetProvider().GetScriptMessageRouter());
    ASSERT_TRUE(config);
    ASSERT_TRUE(router);
  }

  // No configuration and router after |Purge| call.
  GetProvider().Purge();
  EXPECT_FALSE(config);
  EXPECT_FALSE(router);
}

// Tests that configuration's userContentController has only one script with the
// same content as web::GetEarlyPageScript() returns.
TEST_F(WKWebViewConfigurationProviderTest, UserScript) {
  WKWebViewConfiguration* config = GetProvider().GetWebViewConfiguration();
  NSArray* scripts = config.userContentController.userScripts;
  EXPECT_EQ(1U, scripts.count);
  NSString* early_script = GetEarlyPageScript();
  // |earlyScript| is a substring of |userScripts|. The latter wraps the
  // former with "if (!injected)" check to avoid double injections.
  EXPECT_LT(0U, [[scripts[0] source] rangeOfString:early_script].length);
}

}  // namespace
}  // namespace web
