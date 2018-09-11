// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/autofill/automation/automation_action.h"

#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/js_test_util.h"

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
// Right now this always assumes a click event of the following format:
//  {
//    "selectorType": "xpath",
//    "selector": "//*[@id=\"add-to-cart-button\"]",
//    "context": [],
//    "type": "click"
//  }
@interface AutomationActionClick : AutomationAction
@end

// An action that waits for a series of JS assertions to become true before
// continuing. We assume this action has a format resembling:
// {
//   "context": [],
//   "type": "waitFor",
//   "assertions": ["return document.querySelector().style.display ===
//   'none';"]
// }
@interface AutomationActionWaitFor : AutomationAction
@end

// An action that performs autofill on a form by selecting an element
// that is part of an autofillable form, then tapping the relevant
// autofill suggestion. We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "type": "autofill"
// }
@interface AutomationActionAutofill : AutomationAction
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
    @"waitFor" : [AutomationActionWaitFor class],
    @"autofill" : [AutomationActionAutofill class],
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

// A shared flow across many actions, this waits for the target element to be
// visible, scrolls it into view, then taps on it.
- (void)tapOnTarget:(web::test::ElementSelector)selector {
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

// Creates a selector targeting the element specified in the action.
- (web::test::ElementSelector)selectorForTarget {
  const base::Value* xpathValue(self.actionDictionary->FindKeyOfType(
      "selector", base::Value::Type::STRING));
  GREYAssert(xpathValue, @"Selector is missing in action.");

  const std::string xpath(xpathValue->GetString());
  GREYAssert(!xpath.empty(), @"selector is an empty value.");

  // Creates a selector from the action dictionary.
  web::test::ElementSelector selector(
      ElementSelector::ElementSelectorXPath(xpath));
  return selector;
}

@end

@implementation AutomationActionClick

- (void)execute {
  web::test::ElementSelector selector = [self selectorForTarget];
  [self tapOnTarget:selector];
}

@end

@implementation AutomationActionWaitFor

- (void)execute {
  const base::Value* assertionsValue(self.actionDictionary->FindKeyOfType(
      "assertions", base::Value::Type::LIST));
  GREYAssert(assertionsValue, @"Assertions key is missing in action.");

  const base::Value::ListStorage& assertionsValues(assertionsValue->GetList());
  GREYAssert(assertionsValues.size(), @"Assertions list is empty.");

  std::vector<std::string> state_assertions;

  for (auto const& assertionValue : assertionsValues) {
    const std::string assertionString(assertionValue.GetString());
    GREYAssert(!assertionString.empty(), @"assertionString is an empty value.");
    state_assertions.push_back(assertionString);
  }

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^{
                   return [self CheckForJsAssertionFailures:state_assertions] ==
                          nil;
                 }),
             @"waitFor State change hasn't completed within timeout.");
}

// Executes a vector of Javascript assertions on the webpage, returning the
// first assertion that fails to be true, or nil if all assertions are true.
- (NSString*)CheckForJsAssertionFailures:
    (const std::vector<std::string>&)assertions {
  for (std::string const& assertion : assertions) {
    NSError* error;
    NSString* assertionString =
        [NSString stringWithUTF8String:assertion.c_str()];

    id result = chrome_test_util::ExecuteJavaScript(
        [NSString stringWithFormat:@""
                                    "    (function() {"
                                    "      try {"
                                    "        %@"
                                    "      } catch (ex) {}"
                                    "      return false;"
                                    "    })();",
                                   assertionString],
        &error);

    if (![result boolValue] || error) {
      return assertionString;
    }
  }
  return nil;
}

@end

@implementation AutomationActionAutofill

static const char PROFILE_NAME_FULL[] = "Yuki Nagato";
static const char PROFILE_HOME_LINE1[] = "1600 Amphitheatre Parkway";
static const char PROFILE_HOME_CITY[] = "Mountain View";
static const char PROFILE_HOME_STATE[] = "CA";
static const char PROFILE_HOME_ZIP[] = "94043";

// Loads the predefined autofill profile into the personal data manager, so that
// autofill actions will be suggested when tapping on an autofillable form.
- (void)prepareAutofillProfileWithWebState:(web::WebState*)web_state {
  autofill::AutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebState(web_state)->autofill_manager();
  autofill::PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  profile.SetRawInfo(autofill::NAME_FULL, base::UTF8ToUTF16(PROFILE_NAME_FULL));
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                     base::UTF8ToUTF16(PROFILE_HOME_LINE1));
  profile.SetRawInfo(autofill::ADDRESS_HOME_CITY,
                     base::UTF8ToUTF16(PROFILE_HOME_CITY));
  profile.SetRawInfo(autofill::ADDRESS_HOME_STATE,
                     base::UTF8ToUTF16(PROFILE_HOME_STATE));
  profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                     base::UTF8ToUTF16(PROFILE_HOME_ZIP));
  personal_data_manager->SaveImportedProfile(profile);
}

- (void)execute {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  [self prepareAutofillProfileWithWebState:web_state];

  web::test::ElementSelector selector = [self selectorForTarget];
  [self tapOnTarget:selector];

  // Tap on the autofill suggestion to perform the actual autofill.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormSuggestionLabelAccessibilityIdentifier)]
      performAction:grey_tap()];
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
