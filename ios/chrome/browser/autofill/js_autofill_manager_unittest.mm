// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#import <Foundation/Foundation.h>

#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/619982): MobileSafari corrected HTMLInputElement.maxLength
// with the specification ( https://bugs.webkit.org/show_bug.cgi?id=154906 ).
// Add support for old and new default maxLength value until we dropped Xcode 7.
NSNumber* GetDefaultMaxLength() {
  return @524288;
}

// Text fixture to test JsAutofillManager.
class JsAutofillManagerTest : public ChromeWebTest {
 protected:
  // Loads the given HTML and initializes the Autofill JS scripts.
  void LoadHtml(NSString* html) {
    ChromeWebTest::LoadHtml(html);
    CRWJSInjectionManager* manager = [web_state()->GetJSInjectionReceiver()
        instanceOfClass:[JsAutofillManager class]];
    manager_ = static_cast<JsAutofillManager*>(manager);
    [manager_ inject];
  }
  // Testable autofill manager.
  JsAutofillManager* manager_;
};

// Tests that |hasBeenInjected| returns YES after |inject| call.
TEST_F(JsAutofillManagerTest, InitAndInject) {
  LoadHtml(@"<html></html>");
  EXPECT_TRUE([manager_ hasBeenInjected]);
}

// Tests forms extraction method
// (fetchFormsWithRequirements:minimumRequiredFieldsCount:completionHandler:).
TEST_F(JsAutofillManagerTest, ExtractForms) {
  LoadHtml(@"<html><body><form name='testform' method='post'>"
            "<input type='text' name='firstname'/>"
            "<input type='text' name='lastname'/>"
            "<input type='email' name='email'/>"
            "</form></body></html>");

  NSDictionary* expected = @{
    @"name" : @"testform",
    @"method" : @"post",
    @"fields" : @[
      @{
        @"name" : @"firstname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      },
      @{
        @"name" : @"lastname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      },
      @{
        @"name" : @"email",
        @"form_control_type" : @"email",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      }
    ]
  };

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::kRequiredFieldsForPredictionRoutines
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  NSDictionary* resultDict = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultDict);

  NSDictionary* forms = [resultDict[@"forms"] firstObject];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(forms[key], obj);
  }];
}

// Tests form filling (fillActiveFormField:completionHandler:) method.
TEST_F(JsAutofillManagerTest, FillActiveFormField) {
  LoadHtml(@"<html><body><form name='testform' method='post'>"
            "<input type='email' name='email'/>"
            "</form></body></html>");

  NSString* get_element_javascript = @"document.getElementsByName('email')[0]";
  NSString* focus_element_javascript =
      [NSString stringWithFormat:@"%@.focus()", get_element_javascript];
  [manager_ executeJavaScript:focus_element_javascript completionHandler:nil];
  [manager_
      fillActiveFormField:@"{\"name\":\"email\",\"value\":\"newemail@com\"}"
        completionHandler:^{
        }];

  NSString* element_value_javascript =
      [NSString stringWithFormat:@"%@.value", get_element_javascript];
  EXPECT_NSEQ(@"newemail@com",
              web::ExecuteJavaScript(manager_, element_value_javascript));
}

}  // namespace
