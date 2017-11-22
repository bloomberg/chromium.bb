// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/fake_js_autofill_manager.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/fakes/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_state/form_activity_params.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::kWaitForActionTimeout;
using testing::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {

NSString* const kTestFormName = @"FormName";
NSString* const kTestFieldName = @"FieldName";
NSString* const kTestFieldValue = @"FieldValue";

}  // namespace

class CWVAutofillControllerTest : public PlatformTest {
 protected:
  CWVAutofillControllerTest()
      : browser_state_(/*off_the_record=*/false),
        autofill_agent_([[FakeAutofillAgent alloc]
            initWithPrefService:browser_state_.GetPrefs()
                       webState:&web_state_]),
        js_autofill_manager_([[FakeJSAutofillManager alloc] init]) {
    l10n_util::OverrideLocaleWithCocoaLocale();

    web_state_.SetBrowserState(&browser_state_);
    CRWTestJSInjectionReceiver* injectionReceiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state_.SetJSInjectionReceiver(injectionReceiver);

    autofill_controller_ =
        [[CWVAutofillController alloc] initWithWebState:&web_state_
                                          autofillAgent:autofill_agent_
                                      JSAutofillManager:js_autofill_manager_];
  };

  web::TestWebThreadBundle web_thread_bundle_;
  ios_web_view::WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  CWVAutofillController* autofill_controller_;
  FakeAutofillAgent* autofill_agent_;
  FakeJSAutofillManager* js_autofill_manager_;
};

// Tests CWVAutofillController fetch suggestions.
TEST_F(CWVAutofillControllerTest, FetchSuggestions) {
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0];
  [autofill_agent_ addSuggestion:suggestion
                     forFormName:kTestFormName
                       fieldName:kTestFieldName];

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* suggestion = suggestions.firstObject;
    EXPECT_NSEQ(kTestFieldValue, suggestion.value);
    EXPECT_NSEQ(kTestFormName, suggestion.formName);
    EXPECT_NSEQ(kTestFieldName, suggestion.fieldName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                              fieldName:kTestFieldName
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));
}

// Tests CWVAutofillController fills suggestion.
TEST_F(CWVAutofillControllerTest, FillSuggestion) {
  FormSuggestion* form_suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:form_suggestion
                                                   formName:kTestFormName
                                                  fieldName:kTestFieldName];
  __block BOOL fill_completion_was_called = NO;
  [autofill_controller_ fillSuggestion:suggestion
                     completionHandler:^{
                       fill_completion_was_called = YES;
                     }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fill_completion_was_called;
  }));
  EXPECT_NSEQ(form_suggestion,
              [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                                   fieldName:kTestFieldName]);
}

// Tests CWVAutofillController clears form.
TEST_F(CWVAutofillControllerTest, ClearForm) {
  __block BOOL clear_form_completion_was_called = NO;
  [autofill_controller_ clearFormWithName:kTestFormName
                        completionHandler:^{
                          clear_form_completion_was_called = YES;
                        }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return clear_form_completion_was_called;
  }));
  EXPECT_NSEQ(kTestFormName, js_autofill_manager_.lastClearedFormName);
}

// Tests CWVAutofillController delegate focus callback is invoked.
TEST_F(CWVAutofillControllerTest, FocusCallback) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |autofill_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    web::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_name = base::SysNSStringToUTF8(kTestFieldName);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.type = "focus";

    [[delegate expect] autofillController:autofill_controller_
                  didFocusOnFieldWithName:kTestFieldName
                                 formName:kTestFormName
                                    value:kTestFieldValue];
  }
}

// Tests CWVAutofillController delegate input callback is invoked.
TEST_F(CWVAutofillControllerTest, InputCallback) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |autofill_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    web::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_name = base::SysNSStringToUTF8(kTestFieldName);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.type = "input";

    [[delegate expect] autofillController:autofill_controller_
                  didInputInFieldWithName:kTestFieldName
                                 formName:kTestFormName
                                    value:kTestFieldValue];
  }
}

// Tests CWVAutofillController delegate blur callback is invoked.
TEST_F(CWVAutofillControllerTest, BlurCallback) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |autofill_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    web::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_name = base::SysNSStringToUTF8(kTestFieldName);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.type = "blur";

    [[delegate expect] autofillController:autofill_controller_
                   didBlurOnFieldWithName:kTestFieldName
                                 formName:kTestFormName
                                    value:kTestFieldValue];
  }
}

}  // namespace ios_web_view
