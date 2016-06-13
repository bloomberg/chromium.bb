// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

#include <stddef.h>
#import <Foundation/Foundation.h>

#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "testing/gtest_mac.h"

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

namespace web {

// Test fixture to test web controller injection.
class JsInjectionManagerTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    // Loads a dummy page to prepare JavaScript evaluation.
    NSString* const kPageContent = @"<html><body><div></div></body></html>";
    LoadHtml(kPageContent);
  }
  // Returns the manager of the given class.
  CRWJSInjectionManager* GetInstanceOfClass(Class jsInjectionManagerClass);
  // Returns true if the receiver_ has all the managers in |managers|.
  bool HasReceiverManagers(NSArray* managers);
  // EXPECTs that |actual| consists of the CRWJSInjectionManagers of the
  // expected classes in a correct order.
  void TestAllDependencies(NSArray* expected, NSArray* actual);
};

bool JsInjectionManagerTest::HasReceiverManagers(NSArray* manager_classes) {
  NSDictionary* receiver_managers =
      [web_state()->GetJSInjectionReceiver() managers];
  for (Class manager_class in manager_classes) {
    if (![receiver_managers objectForKey:manager_class])
      return false;
  }
  return true;
}

void JsInjectionManagerTest::TestAllDependencies(NSArray* expected_classes,
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

CRWJSInjectionManager* JsInjectionManagerTest::GetInstanceOfClass(
    Class jsInjectionManagerClass) {
  return [web_state()->GetJSInjectionReceiver()
      instanceOfClass:jsInjectionManagerClass];
}

TEST_F(JsInjectionManagerTest, NoDependencies) {
  NSUInteger originalCount =
      [[web_state()->GetJSInjectionReceiver() managers] count];
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingCRWJSBaseManager class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 1U,
            [[web_state()->GetJSInjectionReceiver() managers] count]);
  EXPECT_TRUE(HasReceiverManagers(@[ [TestingCRWJSBaseManager class] ]));
  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
}

TEST_F(JsInjectionManagerTest, HasDependencies) {
  NSUInteger originalCount =
      [[web_state()->GetJSInjectionReceiver() managers] count];
  CRWJSInjectionManager* manager = GetInstanceOfClass([TestingJsManager class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 2U,
            [[web_state()->GetJSInjectionReceiver() managers] count])
      << "Two more CRWJSInjectionManagers should be created.";
  EXPECT_TRUE(HasReceiverManagers(
      @[ [TestingCRWJSBaseManager class], [TestingCRWJSBaseManager class] ]));

  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  EXPECT_TRUE(
      [GetInstanceOfClass([TestingCRWJSBaseManager class]) hasBeenInjected]);
}

TEST_F(JsInjectionManagerTest, Dynamic) {
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingDynamicJsManager class]);
  EXPECT_TRUE(manager);

  EXPECT_FALSE([manager hasBeenInjected]);
  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  // Ensure that content isn't cached.
  EXPECT_NSNE([manager injectionContent], [manager injectionContent]);
}

TEST_F(JsInjectionManagerTest, HasNestedDependencies) {
  NSUInteger originalCount =
      [[web_state()->GetJSInjectionReceiver() managers] count];
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingJsManagerWithNestedDependencies class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 3U,
            [[web_state()->GetJSInjectionReceiver() managers] count])
      << "Three more CRWJSInjectionManagers should be created.";
  EXPECT_TRUE(HasReceiverManagers(@[
    [TestingJsManagerWithNestedDependencies class],
    [TestingCRWJSBaseManager class], [TestingCRWJSBaseManager class]
  ]));

  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  EXPECT_TRUE([GetInstanceOfClass([TestingJsManager class]) hasBeenInjected]);
  EXPECT_TRUE(
      [GetInstanceOfClass([TestingCRWJSBaseManager class]) hasBeenInjected]);

  NSArray* list = [manager allDependencies];
  TestAllDependencies(
      @[
        [TestingCRWJSBaseManager class], [TestingJsManager class],
        [TestingJsManagerWithNestedDependencies class]
      ],
      list);
}

// Tests that checking for an uninjected presence beacon returns false.
TEST_F(JsInjectionManagerTest, WebControllerCheckForUninjectedScript) {
  EXPECT_FALSE([web_state()->GetJSInjectionReceiver()
      scriptHasBeenInjectedForClass:Nil
                     presenceBeacon:@"__gCrWeb.dummyBeacon"]);
}

TEST_F(JsInjectionManagerTest, AllDependencies) {
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingJsManagerComplex class]);
  NSArray* list = [manager allDependencies];
  TestAllDependencies(
      @[
        [TestingCRWJSBaseManager class], [TestingAnotherCRWJSBaseManager class],
        [TestingJsManager class], [TestingAnotherJsManager class],
        [TestingJsManagerWithNestedDependencies class],
        [TestingJsManagerComplex class]
      ],
      list);
}

}  // namespace web
