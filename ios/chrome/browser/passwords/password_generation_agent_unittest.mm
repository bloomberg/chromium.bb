// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_agent.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/passwords/js_password_manager.h"
#import "ios/chrome/browser/passwords/password_generation_offer_view.h"
#import "ios/chrome/browser/passwords/passwords_ui_delegate.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kAccountCreationFormName = @"create-foo-account";
NSString* const kAccountCreationFieldName = @"password";
NSString* const kAccountCreationOrigin = @"http://foo.com/login";
NSString* const kEmailFieldName = @"email";

}  // namespace

@interface MockPasswordsUiDelegate : NSObject<PasswordsUiDelegate>

- (instancetype)init;

@property(nonatomic, readonly) BOOL UIShown;

@end

@implementation MockPasswordsUiDelegate {
  // YES if showGenerationAlertWithPassword was called more recently than
  // hideGenerationAlert, NO otherwise.
  BOOL _UIShown;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _UIShown = NO;
  }
  return self;
}

@synthesize UIShown = _UIShown;

- (void)showGenerationAlertWithPassword:(NSString*)password
                      andPromptDelegate:
                          (id<PasswordGenerationPromptDelegate>)delegate {
  _UIShown = YES;
}

- (void)hideGenerationAlert {
  _UIShown = NO;
}

@end

namespace {

// Returns a form that should be marked as an account creation form by local
// heuristics.
autofill::PasswordForm GetAccountCreationForm() {
  autofill::FormFieldData name_field;
  name_field.name = base::ASCIIToUTF16("name");
  name_field.form_control_type = "text";

  autofill::FormFieldData email_field;
  email_field.name = base::ASCIIToUTF16("email");
  email_field.form_control_type = "email";

  autofill::FormFieldData password_field;
  password_field.name = base::SysNSStringToUTF16(kAccountCreationFieldName);
  password_field.form_control_type = "password";

  autofill::FormFieldData confirmPasswordField;
  confirmPasswordField.name = base::ASCIIToUTF16("confirm");
  confirmPasswordField.form_control_type = "password";

  autofill::FormData form;
  form.name = base::SysNSStringToUTF16(kAccountCreationFormName);
  form.origin = GURL(base::SysNSStringToUTF8(kAccountCreationOrigin));
  form.action = GURL(base::SysNSStringToUTF8(kAccountCreationOrigin));

  form.fields.push_back(name_field);
  form.fields.push_back(email_field);
  form.fields.push_back(password_field);
  form.fields.push_back(confirmPasswordField);

  autofill::PasswordForm password_form;
  password_form.origin = form.origin;
  password_form.username_element = email_field.name;
  password_form.password_element = password_field.name;

  password_form.form_data = form;

  return password_form;
}

// Executes each block in |blocks|, where each block must have the type
// void^(void).
void ExecuteBlocks(NSArray* blocks) {
  for (void (^block)(void) in blocks)
    block();
}

// Returns a form that has the same origin as GAIA.
autofill::PasswordForm GetGAIAForm() {
  autofill::PasswordForm form(GetAccountCreationForm());
  form.origin = GaiaUrls::GetInstance()->gaia_login_form_realm();
  form.signon_realm = form.origin.GetOrigin().spec();
  return form;
}

// Returns a form with no text fields.
autofill::PasswordForm GetFormWithNoTextFields() {
  autofill::PasswordForm form(GetAccountCreationForm());
  form.form_data.fields.clear();
  return form;
}

// Returns true if |field| has type "password" and false otherwise.
bool IsPasswordField(const autofill::FormFieldData& field) {
  return field.form_control_type == "password";
}

// Returns all password fields in |form|.
std::vector<autofill::FormFieldData> GetPasswordFields(
    const autofill::PasswordForm& form) {
  std::vector<autofill::FormFieldData> fields;
  fields.reserve(form.form_data.fields.size());
  for (const auto& field : form.form_data.fields) {
    if (IsPasswordField(field))
      fields.push_back(field);
  }
  return fields;
}

// Returns a form with no password fields.
autofill::PasswordForm GetFormWithNoPasswordFields() {
  autofill::PasswordForm form(GetAccountCreationForm());
  form.form_data.fields.erase(
      std::remove_if(form.form_data.fields.begin(), form.form_data.fields.end(),
                     &IsPasswordField),
      form.form_data.fields.end());
  return form;
}

// Test fixture for testing PasswordGenerationAgent.
class PasswordGenerationAgentTest : public web::WebTestWithWebState {
 public:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    mock_js_suggestion_manager_ =
        [OCMockObject niceMockForClass:[JsSuggestionManager class]];
    mock_js_password_manager_ =
        [OCMockObject niceMockForClass:[JsPasswordManager class]];
    mock_ui_delegate_ = [[MockPasswordsUiDelegate alloc] init];
    test_web_state_ = base::MakeUnique<web::TestWebState>();
    dispatcher_ = OCMProtocolMock(@protocol(ApplicationCommands));
    agent_ = [[PasswordGenerationAgent alloc]
             initWithWebState:test_web_state_.get()
              passwordManager:nullptr
        passwordManagerDriver:nullptr
            JSPasswordManager:mock_js_password_manager_
          JSSuggestionManager:mock_js_suggestion_manager_
          passwordsUiDelegate:mock_ui_delegate_
                   dispatcher:dispatcher_];
    @autoreleasepool {
      accessory_view_controller_ = [[FormInputAccessoryViewController alloc]
          initWithWebState:test_web_state_.get()
                 providers:@[ agent_ ]];
    }
  }

  // Sends form data, autofill data, and password manager data to the
  // generation agent so that it can find an account creation form and password
  // field.
  void LoadAccountCreationForm() {
    autofill::PasswordForm password_form(GetAccountCreationForm());
    [agent() allowPasswordGenerationForForm:password_form];
    std::vector<autofill::PasswordForm> password_forms;
    password_forms.push_back(password_form);
    [agent() processParsedPasswordForms:password_forms];
    SetCurrentURLAndTrustLevel(
        GURL(base::SysNSStringToUTF8(kAccountCreationOrigin)),
        web::URLVerificationTrustLevel::kAbsolute);
    SetContentIsHTML(YES);
  }

  // Sets up the web controller mock to use the specified URL and trust level.
  void SetCurrentURLAndTrustLevel(
      GURL url,
      web::URLVerificationTrustLevel url_trust_level) {
    test_web_state_->SetCurrentURL(url);
    test_web_state_->SetTrustLevel(url_trust_level);
  }

  // Swizzles the current web controller to set whether the content is HTML.
  void SetContentIsHTML(BOOL content_is_html) {
    test_web_state_->SetContentIsHTML(content_is_html);
  }

  // Simulates an event on the specified form/field.
  void SimulateFormActivity(NSString* form_name,
                            NSString* field_name,
                            NSString* type) {
    [accessory_view_controller_ webState:test_web_state_.get()
        didRegisterFormActivityWithFormNamed:base::SysNSStringToUTF8(form_name)
                                   fieldName:base::SysNSStringToUTF8(field_name)
                                        type:base::SysNSStringToUTF8(type)
                                       value:""
                                inputMissing:false];
  }

  // Returns a mock of JsSuggestionManager.
  id mock_js_suggestion_manager() { return mock_js_suggestion_manager_; }

  // Returns a mock of JsPasswordManager.
  id mock_js_password_manager() { return mock_js_password_manager_; }

  MockPasswordsUiDelegate* mock_ui_delegate() { return mock_ui_delegate_; }

 protected:
  // Returns the current generation agent.
  PasswordGenerationAgent* agent() { return agent_; }

  // Returns the current accessory view controller.
  FormInputAccessoryViewController* accessory_controller() {
    return accessory_view_controller_;
  }

  // Returns the current dispatcher for the generation agent.
  id dispatcher() { return dispatcher_; }

 private:
  // Test WebState.
  std::unique_ptr<web::TestWebState> test_web_state_;

  // Mock for JsSuggestionManager;
  id mock_js_suggestion_manager_;

  // Mock for JsPasswordManager.
  id mock_js_password_manager_;

  // Mock for the UI delegate.
  MockPasswordsUiDelegate* mock_ui_delegate_;

  // Controller that shows custom input accessory views.
  FormInputAccessoryViewController* accessory_view_controller_;

  // The current generation agent.
  PasswordGenerationAgent* agent_;

  // Dispatcher for the generation agent.
  id dispatcher_;
};

// Tests that local heuristics skip forms with GAIA realm.
TEST_F(PasswordGenerationAgentTest,
       OnParsedForms_ShouldIgnoreFormsWithGaiaRealm) {
  // Send only a form with GAIA origin to the agent.
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(GetGAIAForm());
  [agent() processParsedPasswordForms:forms];

  // No account creation form should have been found.
  EXPECT_FALSE(agent().possibleAccountCreationForm);
  EXPECT_TRUE(agent().passwordFields.empty());
}

// Tests that local heuristics skip forms with no text fields.
TEST_F(PasswordGenerationAgentTest,
       OnParsedForms_ShouldIgnoreFormsWithNotEnoughTextFields) {
  // Send only a form with GAIA origin to the agent.
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(GetFormWithNoTextFields());
  [agent() processParsedPasswordForms:forms];

  // No account creation form should have been found.
  EXPECT_FALSE(agent().possibleAccountCreationForm);
  EXPECT_TRUE(agent().passwordFields.empty());
}

// Tests that local heuristics skip forms with no password fields.
TEST_F(PasswordGenerationAgentTest,
       OnParsedForms_ShouldIgnoreFormsWithNoPasswordFields) {
  // Send only a form with GAIA origin to the agent.
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(GetFormWithNoPasswordFields());
  [agent() processParsedPasswordForms:forms];

  // No account creation form should have been found.
  EXPECT_FALSE(agent().possibleAccountCreationForm);
  EXPECT_TRUE(agent().passwordFields.empty());
}

// Tests that local heuristics extract an account creation form from the page
// when one exists, along with its password fields.
TEST_F(PasswordGenerationAgentTest, OnParsedForms) {
  // Send several forms. One should be selected.
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(GetGAIAForm());
  forms.push_back(GetFormWithNoTextFields());
  forms.push_back(GetFormWithNoPasswordFields());
  forms.push_back(GetAccountCreationForm());
  [agent() processParsedPasswordForms:forms];

  // Should have found an account creation form and extracted its password
  // fields.
  EXPECT_EQ(forms[3], *agent().possibleAccountCreationForm);
  std::vector<autofill::FormFieldData> expectedPasswordFields(
      GetPasswordFields(forms[3]));
  EXPECT_EQ(expectedPasswordFields.size(), agent().passwordFields.size());
  for (size_t i = 0; i < expectedPasswordFields.size(); ++i) {
    EXPECT_FORM_FIELD_DATA_EQUALS(expectedPasswordFields[i],
                                  agent().passwordFields[i]);
  }
}

// Tests that password generation field identification waits until it has
// approval from autofill and the password manager and an account creation
// form has been identified with local heuristics..
TEST_F(PasswordGenerationAgentTest, DeterminePasswordGenerationField) {
  std::vector<autofill::PasswordForm> forms;
  forms.push_back(GetAccountCreationForm());

  autofill::PasswordForm form(GetAccountCreationForm());
  std::vector<autofill::FormFieldData> passwordFields(GetPasswordFields(form));

  // The signals can be received in any order, so test them accordingly by
  // breaking the steps into blocks and executing them in different orders.
  id sendForms = ^{
    std::vector<autofill::PasswordForm> forms;
    forms.push_back(form);
    [agent() processParsedPasswordForms:forms];
  };
  id sendPasswordManagerWhitelist = ^{
    [agent() allowPasswordGenerationForForm:form];
  };
  id expectFieldNotFound = ^{
    EXPECT_FALSE(agent().passwordGenerationField);
  };
  id expectFieldFound = ^{
    // When there are multiple password fields in the account creation form,
    // the first one is used as the generation field.
    EXPECT_FORM_FIELD_DATA_EQUALS(passwordFields[0],
                                  (*agent().passwordGenerationField));
  };

  // For each permutation of steps, the field should only be set after the third
  // signal is received.
  @autoreleasepool {
    ExecuteBlocks(@[
      sendForms, expectFieldNotFound, sendPasswordManagerWhitelist,
      expectFieldFound
    ]);
    [agent() clearState];

    ExecuteBlocks(@[
      sendPasswordManagerWhitelist, expectFieldNotFound, sendForms,
      expectFieldFound
    ]);
    [agent() clearState];
  }
}

// Tests that the password generation UI is shown when the user focuses the
// password field in the account creation form.
TEST_F(PasswordGenerationAgentTest,
       ShouldStartGenerationWhenPasswordFieldFocused) {
  LoadAccountCreationForm();
  id mock = [OCMockObject partialMockForObject:accessory_controller()];
  [[mock expect] showCustomInputAccessoryView:[OCMArg any]];
  SimulateFormActivity(kAccountCreationFormName, kAccountCreationFieldName,
                       @"focus");

  EXPECT_OCMOCK_VERIFY(mock);
  [mock stopMocking];
}

// Tests that requesting password generation shows the alert UI.
TEST_F(PasswordGenerationAgentTest, ShouldShowAlertWhenGenerationRequested) {
  LoadAccountCreationForm();
  id mock = [OCMockObject partialMockForObject:accessory_controller()];
  [[mock expect] showCustomInputAccessoryView:[OCMArg any]];
  SimulateFormActivity(kAccountCreationFormName, kAccountCreationFieldName,
                       @"focus");
  EXPECT_EQ(NO, mock_ui_delegate().UIShown);

  [agent() generatePassword];
  EXPECT_EQ(YES, mock_ui_delegate().UIShown);

  EXPECT_OCMOCK_VERIFY(mock);
  [mock stopMocking];
}

// Tests that the password generation UI is hidden when the user changes focus
// from the password field.
TEST_F(PasswordGenerationAgentTest,
       ShouldStopGenerationWhenDifferentFieldFocused) {
  LoadAccountCreationForm();
  id mock = [OCMockObject partialMockForObject:accessory_controller()];
  [[mock expect] showCustomInputAccessoryView:[OCMArg any]];
  SimulateFormActivity(kAccountCreationFormName, kAccountCreationFieldName,
                       @"focus");

  [[mock expect] restoreDefaultInputAccessoryView];
  SimulateFormActivity(kAccountCreationFormName, kEmailFieldName, @"focus");

  EXPECT_OCMOCK_VERIFY(mock);
  [mock stopMocking];
}

// Tests that the password field is filled when the user accepts a generated
// password.
TEST_F(PasswordGenerationAgentTest,
       ShouldFillPasswordFieldAndDismissAlertWhenUserAcceptsGeneratedPassword) {
  LoadAccountCreationForm();
  // Focus the password field to start generation.
  SimulateFormActivity(kAccountCreationFormName, kAccountCreationFieldName,
                       @"focus");
  NSString* password = @"abc";

  [[[mock_js_password_manager() stub] andDo:^(NSInvocation* invocation) {
    __unsafe_unretained void (^completion_handler)(BOOL);
    [invocation getArgument:&completion_handler atIndex:4];
    completion_handler(YES);
  }] fillPasswordForm:kAccountCreationFormName
      withGeneratedPassword:password
          completionHandler:[OCMArg any]];

  [agent() generatePassword];
  EXPECT_EQ(YES, mock_ui_delegate().UIShown);

  [agent() acceptPasswordGeneration:nil];
  EXPECT_EQ(NO, mock_ui_delegate().UIShown);
  EXPECT_OCMOCK_VERIFY(mock_js_password_manager());
}

// Tests that the Save Passwords setting screen is shown when the user taps
// "show saved passwords".
TEST_F(PasswordGenerationAgentTest,
       ShouldShowPasswordsAndDismissAlertWhenUserTapsShow) {
  LoadAccountCreationForm();
  // Focus the password field to start generation.
  SimulateFormActivity(kAccountCreationFormName, kAccountCreationFieldName,
                       @"focus");
  [agent() generatePassword];
  EXPECT_EQ(YES, mock_ui_delegate().UIShown);

  [agent() showSavedPasswords:nil];
  EXPECT_EQ(NO, mock_ui_delegate().UIShown);
  [[dispatcher() verify] showSavePasswordsSettings];
}

}  // namespace
