// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#include "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/app/application_delegate/mock_tab_opener.h"
#include "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class URLOpenerTest : public PlatformTest {
 protected:
  void TearDown() override {
    [main_controller_ stopChromeMain];
    PlatformTest::TearDown();
  }

  MainController* GetMainController() {
    if (!main_controller_) {
      main_controller_ = [[MainController alloc] init];

      id mainTabModel = [OCMockObject mockForClass:[TabModel class]];
      [[mainTabModel stub] resetSessionMetrics];
      [[mainTabModel stub] browserStateDestroyed];
      [[mainTabModel stub] addObserver:[OCMArg any]];
      [[mainTabModel stub] removeObserver:[OCMArg any]];
      [[main_controller_ browserViewInformation] setMainTabModel:mainTabModel];

      id otrTabModel = [OCMockObject mockForClass:[TabModel class]];
      [[otrTabModel stub] resetSessionMetrics];
      [[otrTabModel stub] browserStateDestroyed];
      [[otrTabModel stub] addObserver:[OCMArg any]];
      [[otrTabModel stub] removeObserver:[OCMArg any]];
      [[main_controller_ browserViewInformation] setOtrTabModel:otrTabModel];
      [main_controller_
          setUpAsForegroundedWithBrowserState:GetChromeBrowserState()];
    }
    return main_controller_;
  }

  ios::ChromeBrowserState* GetChromeBrowserState() {
    if (!chrome_browser_state_.get()) {
      TestChromeBrowserState::Builder builder;
      chrome_browser_state_ = builder.Build();
    }
    return chrome_browser_state_.get();
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  MainController* main_controller_;
};

TEST_F(URLOpenerTest, HandleOpenURL) {
  // A set of tests for robustness of
  // application:openURL:options:tabOpener:startupInformation:
  // It verifies that the function handles correctly differents URL parsed by
  // ChromeAppStartupParameters.
  MainController* controller = GetMainController();

  // The array with the different states to tests (active, not active).
  NSArray* applicationStatesToTest = @[ @YES, @NO ];

  // Mock of TabOpening, preventing the creation of a new tab.
  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // The keys for this dictionary is the URL to call openURL:. The value
  // from the key is either YES or NO to indicate if this is a valid URL
  // or not.
  NSNumber* kIsValid = [NSNumber numberWithBool:YES];
  NSNumber* kNotValid = [NSNumber numberWithBool:NO];
  NSDictionary* urlsToTest = [NSDictionary
      dictionaryWithObjectsAndKeys:
          kNotValid, [NSNull null],
          // Tests for http, googlechrome, and chromium scheme URLs.
          kNotValid, @"", kIsValid, @"http://www.google.com/", kIsValid,
          @"https://www.google.com/settings/account/", kIsValid,
          @"googlechrome://www.google.com/", kIsValid,
          @"googlechromes://www.google.com/settings/account/", kIsValid,
          @"chromium://www.google.com/", kIsValid,
          @"chromiums://www.google.com/settings/account/",
          // Google search results page URLs.
          kIsValid, @"https://www.google.com/search?q=pony&"
                     "sugexp=chrome,mod=7&sourceid=chrome&ie=UTF-8",
          kIsValid, @"googlechromes://www.google.com/search?q=pony&"
                     "sugexp=chrome,mod=7&sourceid=chrome&ie=UTF-8",
          // Other protocols.
          kIsValid, @"chromium-x-callback://x-callback-url/open?url=https://"
                     "www.google.com&x-success=http://success",
          kIsValid, @"file://localhost/path/to/file.pdf",
          // Invalid format input URL will be ignored.
          kNotValid, @"this.is.not.a.valid.url",
          // Valid format but invalid data.
          kIsValid, @"this://is/garbage/but/valid", nil];
  NSArray* sourcesToTest = [NSArray
      arrayWithObjects:[NSNull null], @"", @"com.google.GoogleMobile",
                       @"com.google.GooglePlus", @"com.google.SomeOtherProduct",
                       @"com.apple.mobilesafari",
                       @"com.othercompany.otherproduct", nil];
  // See documentation for |annotation| property in
  // UIDocumentInteractionController Class Reference.  The following values are
  // mostly to detect garbage-in situations and ensure that the app won't crash
  // or garbage out.
  NSArray* annotationsToTest = [NSArray
      arrayWithObjects:[NSNull null],
                       [NSArray arrayWithObjects:@"foo", @"bar", nil],
                       [NSDictionary dictionaryWithObject:@"bar" forKey:@"foo"],
                       @"a string annotation object", nil];
  for (id urlString in [urlsToTest allKeys]) {
    for (id source in sourcesToTest) {
      for (id annotation in annotationsToTest) {
        for (NSNumber* applicationActive in applicationStatesToTest) {
          BOOL applicationIsActive = [applicationActive boolValue];

          controller.startupParameters = nil;
          [tabOpener resetURL];
          NSURL* testUrl = urlString == [NSNull null]
                               ? nil
                               : [NSURL URLWithString:urlString];
          BOOL isValid = [[urlsToTest objectForKey:urlString] boolValue];
          NSMutableDictionary* options = [[NSMutableDictionary alloc] init];
          if (source != [NSNull null]) {
            [options setObject:source
                        forKey:UIApplicationOpenURLOptionsSourceApplicationKey];
          }
          if (annotation != [NSNull null]) {
            [options setObject:annotation
                        forKey:UIApplicationOpenURLOptionsAnnotationKey];
          }
          ChromeAppStartupParameters* params = [ChromeAppStartupParameters
              newChromeAppStartupParametersWithURL:testUrl
                             fromSourceApplication:nil];

          // Action.
          BOOL result = [URLOpener openURL:testUrl
                         applicationActive:applicationIsActive
                                   options:options
                                 tabOpener:tabOpener
                        startupInformation:controller];

          // Tests.
          EXPECT_EQ(isValid, result);
          if (!applicationIsActive) {
            if (result)
              EXPECT_EQ([params externalURL],
                        controller.startupParameters.externalURL);
            else
              EXPECT_EQ(nil, controller.startupParameters);
          } else if (result) {
            EXPECT_EQ([params externalURL], [tabOpener url]);
            tabOpener.completionBlock();
            EXPECT_EQ(nil, controller.startupParameters);
          }
        }
      }
    }
  }
}

// Tests that -handleApplication set startup parameters as expected.
TEST_F(URLOpenerTest, VerifyLaunchOptions) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id appStateMock = [OCMockObject mockForClass:[AppState class]];
  [[appStateMock expect] launchFromURLHandled:YES];

  __block BOOL hasBeenCalled = NO;

  id implementation_block = ^BOOL(
      id self, NSURL* urlArg, BOOL applicationActive, NSDictionary* options,
      id<TabOpening> tabOpener, id<StartupInformation> startupInformation) {
    hasBeenCalled = YES;
    EXPECT_NSEQ([url absoluteString], [urlArg absoluteString]);
    EXPECT_NSEQ(@"com.apple.mobilesafari",
                options[UIApplicationOpenURLOptionsSourceApplicationKey]);
    EXPECT_EQ(startupInformationMock, startupInformation);
    EXPECT_EQ(tabOpenerMock, tabOpener);
    return YES;
  };
  ScopedBlockSwizzler URL_opening_openURL_swizzler([URLOpener class],
                                                   @selector(openURL:
                                                        applicationActive:
                                                                  options:
                                                                tabOpener:
                                                       startupInformation:),
                                                   implementation_block);

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:NO
                       tabOpener:tabOpenerMock
              startupInformation:startupInformationMock
                        appState:appStateMock];

  // Test.
  EXPECT_TRUE(hasBeenCalled);
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}

// Tests that -handleApplication set startup parameters as expected with options
// as nil.
TEST_F(URLOpenerTest, VerifyLaunchOptionsNil) {
  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  // Action.
  [URLOpener handleLaunchOptions:nil
               applicationActive:YES
                       tabOpener:nil
              startupInformation:startupInformationMock
                        appState:appStateMock];
}

// Tests that -handleApplication set startup parameters as expected with no
// source application.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoSourceApplication) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
  };

  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:YES
                       tabOpener:nil
              startupInformation:startupInformationMock
                        appState:appStateMock];
}

// Tests that -handleApplication set startup parameters as expected with no url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoURL) {
  // Setup.
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:YES
                       tabOpener:nil
              startupInformation:startupInformationMock
                        appState:appStateMock];
}

// Tests that -handleApplication set startup parameters as expected with a bad
// url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithBadURL) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium.www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id appStateMock = [OCMockObject mockForClass:[AppState class]];
  [[appStateMock expect] launchFromURLHandled:YES];

  __block BOOL hasBeenCalled = NO;

  id implementation_block = ^BOOL(
      id self, NSURL* urlArg, BOOL applicationActive, NSDictionary* options,
      id<TabOpening> tabOpener, id<StartupInformation> startupInformation) {
    hasBeenCalled = YES;
    EXPECT_NSEQ([url absoluteString], [urlArg absoluteString]);
    EXPECT_NSEQ(@"com.apple.mobilesafari",
                options[UIApplicationOpenURLOptionsSourceApplicationKey]);
    EXPECT_EQ(startupInformationMock, startupInformation);
    EXPECT_EQ(tabOpenerMock, tabOpener);
    return YES;
  };
  ScopedBlockSwizzler URL_opening_openURL_swizzler([URLOpener class],
                                                   @selector(openURL:
                                                        applicationActive:
                                                                  options:
                                                                tabOpener:
                                                       startupInformation:),
                                                   implementation_block);

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:NO
                       tabOpener:tabOpenerMock
              startupInformation:startupInformationMock
                        appState:appStateMock];

  // Test.
  EXPECT_TRUE(hasBeenCalled);
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}
