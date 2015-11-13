// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIWebView.h>

#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/test/web_test.h"
#import "testing/gtest_mac.h"

// Testing class of JsInjectioManager that has no dependencies.
@interface TestingCRWJSBaseManager : CRWJSInjectionManager
@end

@implementation TestingCRWJSBaseManager

- (NSString*)presenceBeacon {
  return @"base";
}

- (NSString*)staticInjectionContent {
  return @"base = {};";
}

@end

// Testing class of JsInjectionManager that has no dependencies.
@interface TestingAnotherCRWJSBaseManager : CRWJSInjectionManager
@end

@implementation TestingAnotherCRWJSBaseManager

- (NSString*)presenceBeacon {
  return @"anotherbase";
}

- (NSString*)staticInjectionContent {
  return @"anotherbase = {};";
}

@end

// Testing class of JsInjectioManager that has dependencies.
@interface TestingJsManager : CRWJSInjectionManager
@end

@implementation TestingJsManager

- (NSString*)presenceBeacon {
  return @"base['testingjs']";
}

- (NSString*)staticInjectionContent {
  return @"base['testingjs'] = {};";
}

- (NSArray*)directDependencies {
  return @[ [TestingCRWJSBaseManager class] ];
}

@end

// Testing class of JsInjectioManager that has dynamic content.
@interface TestingDynamicJsManager : CRWJSInjectionManager
@end

@implementation TestingDynamicJsManager

- (NSString*)presenceBeacon {
  return @"dynamic";
}

- (NSString*)injectionContent {
  static int i = 0;
  return [NSString stringWithFormat:@"dynamic = {}; dynamic['foo'] = %d;", ++i];
}

- (NSString*)staticInjectionContent {
  // This should never be called on a manager that has dynamic content.
  NOTREACHED();
  return nil;
}

@end


// Testing class of JsInjectioManager that has dependencies.
@interface TestingAnotherJsManager : CRWJSInjectionManager
@end

@implementation TestingAnotherJsManager

- (NSString*)presenceBeacon {
  return @"base['anothertestingjs']";
}

- (NSString*)staticInjectionContent {
  return @"base['anothertestingjs'] = {};";
}

- (NSArray*)directDependencies {
  return @[ [TestingCRWJSBaseManager class] ];
}

@end


// Testing class of JsInjectioManager that has nested dependencies.
@interface TestingJsManagerWithNestedDependencies : CRWJSInjectionManager
@end

@implementation TestingJsManagerWithNestedDependencies

- (NSString*)presenceBeacon {
  return @"base['testingjswithnesteddependencies']";
}

- (NSString*)staticInjectionContent {
  return @"base['testingjswithnesteddependencies'] = {};";
}

- (NSArray*)directDependencies {
  return @[[TestingJsManager class]];
}

@end

// Testing class of JsInjectioManager that has nested dependencies.
@interface TestingJsManagerComplex : CRWJSInjectionManager
@end

@implementation TestingJsManagerComplex

- (NSString*)presenceBeacon {
  return @"base['testingjswithnesteddependencies']['complex']";
}

- (NSString*)staticInjectionContent {
  return @"base['testingjswithnesteddependencies']['complex'] = {};";
}

- (NSArray*)directDependencies {
  return @[
    [TestingJsManagerWithNestedDependencies class],
    [TestingAnotherJsManager class],
    [TestingAnotherCRWJSBaseManager class],
  ];
}

@end

#pragma mark -

namespace {

// A mixin class for testing with CRWWKWebViewWebController or
// CRWUIWebViewWebController.
template <typename WebTestT>
class JsInjectionManagerTest : public WebTestT {
 protected:
  virtual void SetUp() override {
    WebTestT::SetUp();
    // Loads a dummy page to prepare JavaScript evaluation.
    NSString* const kPageContent = @"<html><body><div></div></body></html>";
    WebTestT::LoadHtml(kPageContent);
  }
  // Returns the manager of the given class.
  CRWJSInjectionManager* GetInstanceOfClass(Class jsInjectionManagerClass);
  // Returns true if the receiver_ has all the managers in |managers|.
  bool HasReceiverManagers(NSArray* managers);
  // EXPECTs that |actual| consists of the CRWJSInjectionManagers of the
  // expected classes in a correct order.
  void TestAllDependencies(NSArray* expected, NSArray* actual);
};

// Concrete test fixture to test UIWebView-based web controller injection.
typedef JsInjectionManagerTest<web::WebTestWithUIWebViewWebController>
    JsInjectionManagerUIWebViewTest;

// Concrete test fixture to test WKWebView-based web controller injection.
class JsInjectionManagerWKWebViewTest
    : public JsInjectionManagerTest<web::WebTestWithWKWebViewWebController> {
 protected:
  void SetUp() override {
    // SetUp crashes on WKWebView creation if running on iOS7.
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    JsInjectionManagerTest<web::WebTestWithWKWebViewWebController>::SetUp();
  }
};

template <typename WebTestT>
bool JsInjectionManagerTest<WebTestT>::HasReceiverManagers(
    NSArray* manager_classes) {
  NSDictionary* receiver_managers =
      [[WebTestT::webController_ jsInjectionReceiver] managers];
  for (Class manager_class in manager_classes) {
    if (![receiver_managers objectForKey:manager_class])
      return false;
  }
  return true;
}

template <typename WebTestT>
void JsInjectionManagerTest<WebTestT>::TestAllDependencies(
    NSArray* expected_classes,
    NSArray* actual) {
  EXPECT_EQ([expected_classes count], [actual count]);

  for (Class manager_class in expected_classes) {
    CRWJSInjectionManager* expected_manager = GetInstanceOfClass(manager_class);
    EXPECT_TRUE([actual containsObject:expected_manager]);
  }

  for (size_t index = 0; index < [actual count]; ++ index) {
    CRWJSInjectionManager* manager = [actual objectAtIndex:index];
    for (Class manager_class in [manager directDependencies]) {
      CRWJSInjectionManager* dependency = GetInstanceOfClass(manager_class);
      size_t dependency_index = [actual indexOfObject:dependency];
      EXPECT_TRUE(index > dependency_index);
    }
  }
}

template <typename WebTestT>
CRWJSInjectionManager* JsInjectionManagerTest<WebTestT>::GetInstanceOfClass(
    Class jsInjectionManagerClass) {
  return [[WebTestT::webController_ jsInjectionReceiver]
      instanceOfClass:jsInjectionManagerClass];
}

WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           NoDependencies) {
  NSUInteger originalCount =
      [[[this->webController_ jsInjectionReceiver] managers] count];
  CRWJSInjectionManager* manager =
      this->GetInstanceOfClass([TestingCRWJSBaseManager class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 1U,
            [[[this->webController_ jsInjectionReceiver] managers] count]);
  EXPECT_TRUE(this->HasReceiverManagers(@[ [TestingCRWJSBaseManager class] ]));
  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
}

WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           HasDependencies) {
  NSUInteger originalCount =
      [[[this->webController_ jsInjectionReceiver] managers] count];
  CRWJSInjectionManager* manager =
      this->GetInstanceOfClass([TestingJsManager class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 2U,
            [[[this->webController_ jsInjectionReceiver] managers] count])
      << "Two more CRWJSInjectionManagers should be created.";
  EXPECT_TRUE(this->HasReceiverManagers(
      @[ [TestingCRWJSBaseManager class], [TestingCRWJSBaseManager class] ]));

  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  EXPECT_TRUE([this->GetInstanceOfClass([TestingCRWJSBaseManager class])
                  hasBeenInjected]);
}

WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           Dynamic) {
  CRWJSInjectionManager* manager =
      this->GetInstanceOfClass([TestingDynamicJsManager class]);
  EXPECT_TRUE(manager);

  EXPECT_FALSE([manager hasBeenInjected]);
  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  // Ensure that content isn't cached.
  EXPECT_NSNE([manager injectionContent], [manager injectionContent]);
}

WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           HasNestedDependencies) {
  NSUInteger originalCount =
      [[[this->webController_ jsInjectionReceiver] managers] count];
  CRWJSInjectionManager* manager =
      this->GetInstanceOfClass([TestingJsManagerWithNestedDependencies class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 3U,
            [[[this->webController_ jsInjectionReceiver] managers] count])
      << "Three more CRWJSInjectionManagers should be created.";
  EXPECT_TRUE(this->HasReceiverManagers(@[
    [TestingJsManagerWithNestedDependencies class],
    [TestingCRWJSBaseManager class],
    [TestingCRWJSBaseManager class]
  ]));

  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  EXPECT_TRUE(
      [this->GetInstanceOfClass([TestingJsManager class]) hasBeenInjected]);
  EXPECT_TRUE([this->GetInstanceOfClass([TestingCRWJSBaseManager class])
                  hasBeenInjected]);

  NSArray* list = [manager allDependencies];
  this->TestAllDependencies(
      @[
        [TestingCRWJSBaseManager class],
        [TestingJsManager class],
        [TestingJsManagerWithNestedDependencies class]
      ],
      list);
}

// Tests that checking for an uninjected presence beacon returns false.
WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           WebControllerCheckForUninjectedScript) {
  EXPECT_FALSE([this->webController_
      scriptHasBeenInjectedForClass:Nil
                     presenceBeacon:@"__gCrWeb.dummyBeacon"]);
}

WEB_TEST_F(JsInjectionManagerUIWebViewTest,
           JsInjectionManagerWKWebViewTest,
           AllDependencies) {
  CRWJSInjectionManager* manager =
      this->GetInstanceOfClass([TestingJsManagerComplex class]);
  NSArray* list = [manager allDependencies];
  this->TestAllDependencies(
      @[
        [TestingCRWJSBaseManager class],
        [TestingAnotherCRWJSBaseManager class],
        [TestingJsManager class],
        [TestingAnotherJsManager class],
        [TestingJsManagerWithNestedDependencies class],
        [TestingJsManagerComplex class]
      ],
      list);
}

}  // namespace
