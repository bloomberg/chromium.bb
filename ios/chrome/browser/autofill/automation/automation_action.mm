// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/autofill/automation/automation_action.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::ElementSelector;

@interface AutomationAction () {
  std::unique_ptr<const base::DictionaryValue> actionDictionary_;
}

@property(nonatomic, readonly)
    const std::unique_ptr<const base::DictionaryValue>& actionDictionary;

// Selects the proper subclass in the class cluster for the given type. Called
// from the class method creating the actions.
+ (Class)classForType:(NSString*)type;

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary NS_DESIGNATED_INITIALIZER;
@end

// An action that always fails.
@interface AutomationActionUnrecognized : AutomationAction
@end

// An action that simply tap on an element on the page.
@interface AutomationActionClick : AutomationAction
@end

@implementation AutomationAction

+ (instancetype)actionWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  const base::Value* typeValue =
      actionDictionary.FindKeyOfType("type", base::Value::Type::STRING);
  GREYAssert(typeValue, @"Type is missing in action.");

  const std::string type(typeValue->GetString());
  GREYAssert(!type.empty(), @"Type is an empty value.");

  return [[[self classForType:base::SysUTF8ToNSString(type)] alloc]
      initWithValueDictionary:actionDictionary];
}

+ (Class)classForType:(NSString*)type {
  static NSDictionary* classForType = @{
    @"click" : [AutomationActionClick class],
    // More to come.
  };

  return classForType[type] ?: [AutomationActionUnrecognized class];
}

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  self = [super init];
  if (self) {
    actionDictionary_ = actionDictionary.DeepCopyWithoutEmptyChildren();
  }
  return self;
}

- (void)execute {
  GREYAssert(NO, @"Should not be called!");
}

- (const std::unique_ptr<const base::DictionaryValue>&)actionDictionary {
  return actionDictionary_;
}

@end

@implementation AutomationActionClick

- (void)execute {
  // Right now this always assumes a click event of the following format:
  //  {
  //    "selectorType": "xpath",
  //    "selector": "//*[@id=\"add-to-cart-button\"]",
  //    "context": [],
  //    "type": "click"
  //  },

  const base::Value* xpathValue(self.actionDictionary->FindKeyOfType(
      "selector", base::Value::Type::STRING));
  GREYAssert(xpathValue, @"Selector is missing in action.");

  const std::string xpath(xpathValue->GetString());
  GREYAssert(!xpath.empty(), @"selector is an empty value.");

  auto selector(ElementSelector::ElementSelectorXPath(xpath));
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebViewContainingElement:selector];
  // Potentially scroll into view if below the fold.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:WebViewScrollElementToVisible(web_state, selector)];
  // Tap on the element.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:web::WebViewTapElement(web_state, selector)];
}

@end

@implementation AutomationActionUnrecognized

- (void)execute {
  const base::Value* typeValue =
      self.actionDictionary->FindKeyOfType("type", base::Value::Type::STRING);
  const std::string type(typeValue->GetString());

  GREYAssert(NO, @"Unknown action of type %s", type.c_str());
}

@end
