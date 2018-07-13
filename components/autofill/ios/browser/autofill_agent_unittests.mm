// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#include "components/prefs/pref_service.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilCondition;

// Test fixture for AutofillAgent testing.
class AutofillAgentTests : public PlatformTest {
 public:
  AutofillAgentTests() {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Mock CRWJSInjectionReceiver for verifying interactions.
    mock_js_injection_receiver_ =
        [OCMockObject mockForClass:[CRWJSInjectionReceiver class]];
    test_web_state_.SetJSInjectionReceiver(mock_js_injection_receiver_);
    test_web_state_.SetContentIsHTML(true);
    test_web_state_.SetCurrentURL(GURL("https://example.com"));

    prefs_ = autofill::test::PrefServiceForTesting();
    prefs_->SetBoolean(autofill::prefs::kAutofillEnabled, true);
    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:prefs_.get()
                                          webState:&test_web_state_];
  }

  void TearDown() override {
    [autofill_agent_ detachFromWebState];

    PlatformTest::TearDown();
  }

  web::TestWebState test_web_state_;
  std::unique_ptr<PrefService> prefs_;
  AutofillAgent* autofill_agent_;
  id mock_js_injection_receiver_;

  DISALLOW_COPY_AND_ASSIGN(AutofillAgentTests);
};

// Tests that form's name and fields' identifiers, values, and whether they are
// autofilled are sent to the JS. Fields with empty values and those that are
// not autofilled are skipped.
TEST_F(AutofillAgentTests, OnFormDataFilledTest) {
  autofill::FormData form;
  form.origin = GURL("https://myform.com");
  form.action = GURL("https://myform.com/submit");
  form.name = base::ASCIIToUTF16("CC form");

  autofill::FormFieldData field;
  field.form_control_type = "text";
  field.label = base::ASCIIToUTF16("Card number");
  field.name = base::ASCIIToUTF16("number");
  field.id = base::ASCIIToUTF16("number");
  field.value = base::ASCIIToUTF16("number_value");
  field.is_autofilled = true;
  form.fields.push_back(field);
  field.label = base::ASCIIToUTF16("Name on Card");
  field.name = base::ASCIIToUTF16("name");
  field.id = base::ASCIIToUTF16("name");
  field.value = base::ASCIIToUTF16("name_value");
  field.is_autofilled = true;
  form.fields.push_back(field);
  field.label = base::ASCIIToUTF16("Expiry Month");
  field.name = base::ASCIIToUTF16("expiry_month");
  field.id = base::ASCIIToUTF16("expiry_month");
  field.value = base::ASCIIToUTF16("01");
  field.is_autofilled = false;
  form.fields.push_back(field);
  field.label = base::ASCIIToUTF16("Unknown field");
  field.name = base::ASCIIToUTF16("unknown");
  field.id = base::ASCIIToUTF16("unknown");
  field.value = base::ASCIIToUTF16("");
  field.is_autofilled = true;
  form.fields.push_back(field);
  // Fields are in alphabetical order.
  [[mock_js_injection_receiver_ expect]
      executeJavaScript:
          @"__gCrWeb.autofill.fillForm({\"fields\":{\"name\":{\"section\":\"\","
          @"\"value\":\"name_value\"},"
          @"\"number\":{\"section\":\"\",\"value\":\"number_value\"}},"
          @"\"formName\":\"CC form\"}, \"\");"
      completionHandler:[OCMArg any]];
  [autofill_agent_ onFormDataFilled:form];
  test_web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(mock_js_injection_receiver_);
}

// Tests that in the case of conflict in fields' identifiers, the last seen
// value of a given field is used.
TEST_F(AutofillAgentTests, OnFormDataFilledWithNameCollisionTest) {
  autofill::FormData form;
  form.origin = GURL("https://myform.com");
  form.action = GURL("https://myform.com/submit");

  autofill::FormFieldData field;
  field.form_control_type = "text";
  field.label = base::ASCIIToUTF16("State");
  field.name = base::ASCIIToUTF16("region");
  field.id = base::ASCIIToUTF16("region");
  field.value = base::ASCIIToUTF16("California");
  field.is_autofilled = true;
  form.fields.push_back(field);
  field.label = base::ASCIIToUTF16("Other field");
  field.name = base::ASCIIToUTF16("field1");
  field.id = base::ASCIIToUTF16("field1");
  field.value = base::ASCIIToUTF16("value 1");
  field.is_autofilled = true;
  form.fields.push_back(field);
  field.label = base::ASCIIToUTF16("Other field");
  field.name = base::ASCIIToUTF16("field1");
  field.id = base::ASCIIToUTF16("field1");
  field.value = base::ASCIIToUTF16("value 2");
  field.is_autofilled = true;
  form.fields.push_back(field);
  // Fields are in alphabetical order.
  [[mock_js_injection_receiver_ expect]
      executeJavaScript:
          @"__gCrWeb.autofill.fillForm({\"fields\":{\"field1\":{\"section\":"
          @"\"\",\"value\":\"value "
          @"2\"},\"region\":{\"section\":\"\",\"value\":\"California\"}},"
          @"\"formName\":\"\"}, \"\");"
      completionHandler:[OCMArg any]];
  [autofill_agent_ onFormDataFilled:form];
  test_web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(mock_js_injection_receiver_);
}

// Tests that when a user initiated form activity is registered the script to
// extract forms is executed.
TEST_F(AutofillAgentTests, SuggestionsAvailable_UserInitiatedActivity) {
  [[mock_js_injection_receiver_ expect]
      executeJavaScript:@"__gCrWeb.autofill.extractForms(1, true);"
      completionHandler:[OCMArg any]];
  [autofill_agent_ checkIfSuggestionsAvailableForForm:@"form"
                                            fieldName:@"address"
                                      fieldIdentifier:@"address"
                                            fieldType:@"text"
                                                 type:@"focus"
                                           typedValue:@""
                                          isMainFrame:YES
                                       hasUserGesture:YES
                                             webState:&test_web_state_
                                    completionHandler:nil];
  test_web_state_.WasShown();

  EXPECT_OCMOCK_VERIFY(mock_js_injection_receiver_);
}

// Tests that when a non user initiated form activity is registered the
// completion callback passed to the call to check if suggestions are available
// is invoked with no suggestions.
TEST_F(AutofillAgentTests, SuggestionsAvailable_NonUserInitiatedActivity) {
  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;

  [autofill_agent_ checkIfSuggestionsAvailableForForm:@"form"
                                            fieldName:@"address"
                                      fieldIdentifier:@"address"
                                            fieldType:@"text"
                                                 type:@"focus"
                                           typedValue:@""
                                          isMainFrame:YES
                                       hasUserGesture:NO
                                             webState:&test_web_state_
                                    completionHandler:^(BOOL success) {
                                      completion_handler_success = success;
                                      completion_handler_called = YES;
                                    }];
  test_web_state_.WasShown();

  // Wait until the expected handler is called.
  WaitUntilCondition(^bool() {
    return completion_handler_called;
  });
  EXPECT_FALSE(completion_handler_success);
}
