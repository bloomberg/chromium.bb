// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

using testing::ReturnRef;

class SettingsNavigationControllerTest : public PlatformTest {
 protected:
  SettingsNavigationControllerTest()
      : scoped_browser_state_manager_(
            base::MakeUnique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        &AuthenticationServiceFake::CreateAuthenticationService);
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    mockDelegate_.reset([[OCMockObject
        niceMockForProtocol:@protocol(SettingsNavigationControllerDelegate)]
        retain]);

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_.reset(
        [[defaults stringForKey:kSpdyProxyEnabled] copy]);
    [defaults setObject:@"Disabled" forKey:kSpdyProxyEnabled];
  };

  ~SettingsNavigationControllerTest() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_.get()
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::mac::ScopedNSAutoreleasePool pool_;
  base::scoped_nsprotocol<id> mockDelegate_;
  base::scoped_nsobject<NSString> initialValueForSpdyProxyEnabled_;
};

// When navigation stack has more than one view controller,
// -popViewControllerAnimated: successfully removes the top view controller.
TEST_F(SettingsNavigationControllerTest, PopController) {
  base::scoped_nsobject<SettingsNavigationController> settingsController(
      [SettingsNavigationController
          newSettingsMainControllerWithMainBrowserState:chrome_browser_state_
                                                            .get()
                                    currentBrowserState:chrome_browser_state_
                                                            .get()
                                               delegate:nil]);
  base::scoped_nsobject<UIViewController> viewController(
      [[UIViewController alloc] initWithNibName:nil bundle:nil]);
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);

  UIViewController* poppedViewController =
      [settingsController popViewControllerAnimated:NO];
  EXPECT_NSEQ(viewController, poppedViewController);
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
}

// When the navigation stack has only one view controller,
// -popViewControllerAnimated: returns false.
TEST_F(SettingsNavigationControllerTest, DontPopRootController) {
  base::scoped_nsobject<SettingsNavigationController> settingsController(
      [SettingsNavigationController
          newSettingsMainControllerWithMainBrowserState:chrome_browser_state_
                                                            .get()
                                    currentBrowserState:chrome_browser_state_
                                                            .get()
                                               delegate:nil]);
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);

  EXPECT_FALSE([settingsController popViewControllerAnimated:NO]);
}

// When the settings navigation stack has more than one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: pops the top view controller to
// reveal the view controller underneath.
TEST_F(SettingsNavigationControllerTest,
       PopWhenNavigationStackSizeIsGreaterThanOne) {
  base::scoped_nsobject<SettingsNavigationController> settingsController(
      [SettingsNavigationController
          newSettingsMainControllerWithMainBrowserState:chrome_browser_state_
                                                            .get()
                                    currentBrowserState:chrome_browser_state_
                                                            .get()
                                               delegate:mockDelegate_]);
  base::scoped_nsobject<UIViewController> viewController(
      [[UIViewController alloc] initWithNibName:nil bundle:nil]);
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);
  [[mockDelegate_ reject] closeSettings];
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
}

// When the settings navigation stack only has one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: calls -closeSettings on the
// delegate.
TEST_F(SettingsNavigationControllerTest,
       CloseSettingsWhenNavigationStackSizeIsOne) {
  base::scoped_nsobject<SettingsNavigationController> settingsController(
      [SettingsNavigationController
          newSettingsMainControllerWithMainBrowserState:chrome_browser_state_
                                                            .get()
                                    currentBrowserState:chrome_browser_state_
                                                            .get()
                                               delegate:mockDelegate_]);
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  [[mockDelegate_ expect] closeSettings];
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
}

}  // namespace
