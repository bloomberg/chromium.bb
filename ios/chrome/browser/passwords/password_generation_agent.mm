// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_agent.h"

#include <stddef.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/passwords/js_password_manager.h"
#import "ios/chrome/browser/passwords/password_generation_edit_view.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Target length of generated passwords.
const int kGeneratedPasswordLength = 20;

// The minimum number of text fields that a form needs to be considered as
// an account creation form.
const size_t kMinimumTextFieldsForAccountCreation = 3;

// Returns true if |urls| contains |url|.
bool VectorContainsURL(const std::vector<GURL>& urls, const GURL& url) {
  return std::find(urls.begin(), urls.end(), url) != urls.end();
}

// Returns whether |field| should be considered a text field. Implementation
// mirrors that of password_controller.js.
// TODO(crbug.com/433856): Figure out how to determine if |field| is visible.
bool IsTextField(const autofill::FormFieldData& field) {
  return field.form_control_type == "text" ||
         field.form_control_type == "email" ||
         field.form_control_type == "number" ||
         field.form_control_type == "tel" || field.form_control_type == "url" ||
         field.form_control_type == "search" ||
         field.form_control_type == "password";
}

}  // namespace

@interface PasswordGenerationAgent ()<CRWWebStateObserver,
                                      FormInputAccessoryViewProvider,
                                      PasswordGenerationOfferDelegate,
                                      PasswordGenerationPromptDelegate>

// Clears all per-page state.
- (void)clearState;

// Returns YES if |form| belongs to the GAIA realm.
- (BOOL)formHasGAIARealm:(const autofill::PasswordForm&)form;

// Returns YES if |form| contains enough text fields to be considered as an
// account creation form.
- (BOOL)formHasEnoughTextFieldsForAccountCreation:
    (const autofill::PasswordForm&)form;

// Returns a list of all password fields in |form|.
- (std::vector<autofill::FormFieldData>)passwordFieldsInForm:
    (const autofill::PasswordForm&)form;

// Merges the data from local heuristics, the autofill server, and the password
// manager to find the field that should trigger the password generation UI
// when selected by the user. The resulting field is stored in
// |_passwordGenerationField|. This logic is nearly identical to that of the
// upstream autofill::PasswordGenerationAgent::DetermineGenerationElement.
- (void)determinePasswordGenerationField;

// Returns YES if the specified form and field should trigger the
// password generation UI.
- (BOOL)isGenerationForm:(const base::string16&)formName
                   field:(const base::string16&)fieldName;

// The name of the form identified as an account creation form, if it exists.
- (NSString*)passwordGenerationFormName;

// Hides and deletes the alert with generation prompt, if it exists.
- (void)hideAlert;

// Returns an autoreleased input accessory view corresponding to the current
// password generation state. Should only be used when password generation
// should be offered for the currently-focused form field.
- (UIView*)currentAccessoryView;

// Initializes PasswordGenerationAgent, which observes the specified web state,
// and allows injecting JavaScript managers.
- (instancetype)
         initWithWebState:(web::WebState*)webState
          passwordManager:(password_manager::PasswordManager*)passwordManager
    passwordManagerDriver:(password_manager::PasswordManagerDriver*)driver
        JSPasswordManager:(JsPasswordManager*)JSPasswordManager
      JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager
      passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate
    NS_DESIGNATED_INITIALIZER;

@end

@implementation PasswordGenerationAgent {
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // The origin URLs of forms on the current page that have not been blacklisted
  // by the password manager.
  std::vector<GURL> _allowedGenerationFormOrigins;

  // Stores the account creation form we detected on the page.
  std::unique_ptr<autofill::PasswordForm> _possibleAccountCreationForm;

  // Password fields found in |_possibleAccountCreationForm|.
  std::vector<autofill::FormFieldData> _passwordFields;

  // The password field that triggers the password generation UI.
  std::unique_ptr<autofill::FormFieldData> _passwordGenerationField;

  // Wrapper for suggestion JavaScript. Used for form navigation.
  JsSuggestionManager* _suggestionManager;

  // Wrapper for passwords JavaScript. Used for form filling.
  JsPasswordManager* _javaScriptPasswordManager;

  // Driver that is passed to PasswordManager when a password is generated.
  password_manager::PasswordManagerDriver* _passwordManagerDriver;

  // PasswordManager to inform when a password is generated.
  password_manager::PasswordManager* _passwordManager;

  // Callback to update the custom keyboard accessory view. Will be non-nil when
  // this PasswordGenerationAgent controls the keyboard accessory view.
  AccessoryViewReadyCompletion _accessoryViewReadyCompletion;

  // The delegate for controlling the password generation UI.
  id<PasswordsUiDelegate> _passwords_ui_delegate;

  // The password that was generated and accepted by the user.
  NSString* _generatedPassword;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)
     initWithWebState:(web::WebState*)webState
      passwordManager:(password_manager::PasswordManager*)passwordManager
passwordManagerDriver:(password_manager::PasswordManagerDriver*)driver
  passwordsUiDelegate:(id<PasswordsUiDelegate>)delegate {
  JsPasswordManager* javaScriptPasswordManager =
      base::mac::ObjCCast<JsPasswordManager>([webState->GetJSInjectionReceiver()
          instanceOfClass:[JsPasswordManager class]]);
  JsSuggestionManager* suggestionManager =
      base::mac::ObjCCast<JsSuggestionManager>(
          [webState->GetJSInjectionReceiver()
              instanceOfClass:[JsSuggestionManager class]]);
  return [self initWithWebState:webState
                passwordManager:passwordManager
          passwordManagerDriver:driver
              JSPasswordManager:javaScriptPasswordManager
            JSSuggestionManager:suggestionManager
            passwordsUiDelegate:delegate];
}

- (instancetype)
     initWithWebState:(web::WebState*)webState
      passwordManager:(password_manager::PasswordManager*)passwordManager
passwordManagerDriver:(password_manager::PasswordManagerDriver*)driver
    JSPasswordManager:(JsPasswordManager*)javaScriptPasswordManager
  JSSuggestionManager:(JsSuggestionManager*)suggestionManager
  passwordsUiDelegate:(id<PasswordsUiDelegate>)delegate {
  DCHECK([NSThread isMainThread]);
  DCHECK(webState);
  DCHECK_EQ([self class], [PasswordGenerationAgent class]);
  self = [super init];
  if (self) {
    _passwordManager = passwordManager;
    _passwordManagerDriver = driver;
    _javaScriptPasswordManager = javaScriptPasswordManager;
    _suggestionManager = suggestionManager;
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    _passwords_ui_delegate = delegate;
  }
  return self;
}

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
}

- (autofill::PasswordForm*)possibleAccountCreationForm {
  return _possibleAccountCreationForm.get();
}

- (const std::vector<autofill::FormFieldData>&)passwordFields {
  return _passwordFields;
}

- (autofill::FormFieldData*)passwordGenerationField {
  return _passwordGenerationField.get();
}

- (void)clearState {
  [self hideAlert];
  _allowedGenerationFormOrigins.clear();
  _possibleAccountCreationForm.reset();
  _passwordFields.clear();
  _passwordGenerationField.reset();
  _generatedPassword = nil;
}

- (BOOL)formHasGAIARealm:(const autofill::PasswordForm&)form {
  // Do not generate password for GAIA since it is used to retrieve the
  // generated paswords.
  return GURL(form.signon_realm) ==
         GaiaUrls::GetInstance()->gaia_login_form_realm();
}

- (BOOL)formHasEnoughTextFieldsForAccountCreation:
    (const autofill::PasswordForm&)form {
  size_t numVisibleTextFields = 0;
  for (const auto& formFieldData : form.form_data.fields)
    if (IsTextField(formFieldData))
      ++numVisibleTextFields;
  return (numVisibleTextFields >= kMinimumTextFieldsForAccountCreation);
}

- (std::vector<autofill::FormFieldData>)passwordFieldsInForm:
    (const autofill::PasswordForm&)form {
  std::vector<autofill::FormFieldData> passwordFields;
  for (const auto& formFieldData : form.form_data.fields)
    if (formFieldData.form_control_type == "password")
      passwordFields.push_back(formFieldData);
  return passwordFields;
}

- (void)allowPasswordGenerationForForm:(const autofill::PasswordForm&)form {
  _allowedGenerationFormOrigins.push_back(form.origin);
  [self determinePasswordGenerationField];
}

- (void)processParsedPasswordForms:
    (const std::vector<autofill::PasswordForm>&)forms {
  for (const auto& passwordForm : forms) {
    if ([self formHasGAIARealm:passwordForm])
      continue;
    if (![self formHasEnoughTextFieldsForAccountCreation:passwordForm])
      continue;
    std::vector<autofill::FormFieldData> passwordFields(
        [self passwordFieldsInForm:passwordForm]);
    if (passwordFields.empty())
      continue;
    // This form checks out.
    _possibleAccountCreationForm.reset(
        new autofill::PasswordForm(passwordForm));
    _passwordFields = passwordFields;
    break;
  }
  [self determinePasswordGenerationField];
}

- (void)determinePasswordGenerationField {
  // If the current page hasn't been parsed yet or doesn't contain any account
  // creation forms, wait.
  if (!_possibleAccountCreationForm)
    return;
  if (_passwordFields.empty())
    return;

  // If the form origin hasn't been cleared by both the autofill and the
  // password manager, wait.
  GURL origin = _possibleAccountCreationForm->origin;
  if (!experimental_flags::UseOnlyLocalHeuristicsForPasswordGeneration()) {
    if (!VectorContainsURL(_allowedGenerationFormOrigins, origin))
      return;
  }

  // Use the first password field in the form as the generation field.
  _passwordGenerationField.reset(
      new autofill::FormFieldData(_passwordFields[0]));
  autofill::password_generation::LogPasswordGenerationEvent(
      autofill::password_generation::GENERATION_AVAILABLE);
}

- (id<FormInputAccessoryViewProvider>)accessoryViewProvider {
  return self;
}

- (BOOL)isGenerationForm:(const base::string16&)formName
                   field:(const base::string16&)fieldName {
  return _possibleAccountCreationForm &&
         _possibleAccountCreationForm->form_data.name == formName &&
         _passwordGenerationField &&
         _passwordGenerationField->name == fieldName;
}

- (NSString*)passwordGenerationFormName {
  return base::SysUTF16ToNSString(_possibleAccountCreationForm->form_data.name);
}

- (void)hideAlert {
  [_passwords_ui_delegate hideGenerationAlert];
}

- (UIView*)currentAccessoryView {
  return [_generatedPassword length] > 0
             ? [[PasswordGenerationEditView alloc]
                   initWithPassword:_generatedPassword]
             : [[PasswordGenerationOfferView alloc] initWithDelegate:self];
}

#pragma mark -
#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self clearState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self clearState];
  _webStateObserverBridge.reset();
}

#pragma mark -
#pragma mark PasswordGenerationPromptDelegate

- (void)acceptPasswordGeneration:(id)sender {
  [self hideAlert];
  __weak PasswordGenerationAgent* weakSelf = self;
  id completionHandler = ^(BOOL success) {
    if (!success)
      return;
    PasswordGenerationAgent* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    if (strongSelf->_passwordManager) {
      // Might be null in tests.
      strongSelf->_passwordManager->OnPresaveGeneratedPassword(
          *strongSelf->_possibleAccountCreationForm);
    }
    if (strongSelf->_accessoryViewReadyCompletion) {
      strongSelf->_accessoryViewReadyCompletion(
          [strongSelf currentAccessoryView], strongSelf);
    }
  };
  [_javaScriptPasswordManager fillPasswordForm:[self passwordGenerationFormName]
                         withGeneratedPassword:_generatedPassword
                             completionHandler:completionHandler];
}

- (void)showSavedPasswords:(id)sender {
  [self hideAlert];
  GenericChromeCommand* command = [[GenericChromeCommand alloc]
      initWithTag:IDC_SHOW_SAVE_PASSWORDS_SETTINGS];
  [command executeOnMainWindow];
}

#pragma mark -
#pragma mark PasswordGenerationOfferDelegate

- (void)generatePassword {
  _generatedPassword = [base::SysUTF8ToNSString(
      autofill::PasswordGenerator(kGeneratedPasswordLength).Generate()) copy];
  [_passwords_ui_delegate showGenerationAlertWithPassword:_generatedPassword
                                        andPromptDelegate:self];
}

#pragma mark -
#pragma mark FormInputAccessoryViewProvider

- (id<FormInputAccessoryViewDelegate>)accessoryViewDelegate {
  return nil;
}

- (void)setAccessoryViewDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  // Unused.
}

- (void)
    checkIfAccessoryViewIsAvailableForFormNamed:(const std::string&)formName
                                      fieldName:(const std::string&)fieldName
                                       webState:(web::WebState*)webState
                              completionHandler:
                                  (AccessoryViewAvailableCompletion)
                                      completionHandler {
  completionHandler(
      _passwordGenerationField &&
      [self isGenerationForm:base::UTF8ToUTF16(formName)
                       field:base::UTF8ToUTF16(fieldName)]);
}

- (void)retrieveAccessoryViewForFormNamed:(const std::string&)formName
                                fieldName:(const std::string&)fieldName
                                    value:(const std::string&)value
                                     type:(const std::string&)type
                                 webState:(web::WebState*)webState
                 accessoryViewUpdateBlock:
                     (AccessoryViewReadyCompletion)accessoryViewUpdateBlock {
  DCHECK(!_accessoryViewReadyCompletion);
  if ([_generatedPassword length] > 0)
    _generatedPassword = [base::SysUTF8ToNSString(value) copy];
  accessoryViewUpdateBlock([self currentAccessoryView], self);
  _accessoryViewReadyCompletion = [accessoryViewUpdateBlock copy];
}

- (void)inputAccessoryViewControllerDidReset:
    (FormInputAccessoryViewController*)controller {
  [self hideAlert];
  DCHECK(_accessoryViewReadyCompletion);
  _accessoryViewReadyCompletion = nil;
}

- (void)resizeAccessoryView {
  DCHECK(_accessoryViewReadyCompletion);
  _accessoryViewReadyCompletion([self currentAccessoryView], self);
}

- (BOOL)getLogKeyboardAccessoryMetrics {
  // Only store metrics for regular Autofill, not passwords.
  return NO;
}

@end
