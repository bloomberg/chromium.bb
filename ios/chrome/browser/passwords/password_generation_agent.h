// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_AGENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_AGENT_H_

#import <Foundation/Foundation.h>
#include <vector>

#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/passwords/password_generation_offer_view.h"
#import "ios/chrome/browser/passwords/password_generation_prompt_delegate.h"
#import "ios/chrome/browser/passwords/passwords_ui_delegate.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@class CRWWebController;
@protocol FormInputAccessoryViewProvider;
@class JsPasswordManager;
@class JsSuggestionManager;

namespace autofill {
struct FormFieldData;
struct PasswordForm;
}

namespace password_manager {
class PasswordManager;
class PasswordManagerDriver;
}

// Performs password generation for a web state. Locates eligible password
// form fields, manages the user experience flow, and injects generated
// passwords into the page. This is the iOS counterpart to the upstream
// renderer-side PasswordGenerationAgent. This class is not meant to be
// subclassed and should only be used from the main thread.
@interface PasswordGenerationAgent : NSObject

// Initializes PasswordGenerationAgent, which observes the specified web state.
- (instancetype)
         initWithWebState:(web::WebState*)webState
          passwordManager:(password_manager::PasswordManager*)passwordManager
    passwordManagerDriver:(password_manager::PasswordManagerDriver*)driver
      passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate;

- (instancetype)init NS_UNAVAILABLE;

// Indicates that the specified |form| has not been blacklisted by the user
// for the purposes of password management.
- (void)allowPasswordGenerationForForm:(const autofill::PasswordForm&)form;

// Passes the |forms| that were parsed from the current page to this agent so
// that account creation forms and generation fields can be found.
- (void)processParsedPasswordForms:
    (const std::vector<autofill::PasswordForm>&)forms;

// Provides an input accessory view from this PasswordGenerationAgent.
@property(nonatomic, readonly)
    id<FormInputAccessoryViewProvider> accessoryViewProvider;

@end

@interface PasswordGenerationAgent (
    ForTesting)<CRWWebStateObserver,
                FormInputAccessoryViewProvider,
                PasswordGenerationOfferDelegate,
                PasswordGenerationPromptDelegate>

// The form that has been identified by local heuristics as an account creation
// form.
@property(nonatomic, readonly)
    autofill::PasswordForm* possibleAccountCreationForm;

// The password fields in |possibleAccountCreationForm|.
@property(nonatomic, readonly)
    const std::vector<autofill::FormFieldData>& passwordFields;

// The field that triggers the password generation UI.
@property(nonatomic, readonly) autofill::FormFieldData* passwordGenerationField;

// Initializes PasswordGenerationAgent, which observes the specified web state,
// and allows injecting JavaScript managers for testing.
- (instancetype)
         initWithWebState:(web::WebState*)webState
          passwordManager:(password_manager::PasswordManager*)passwordManager
    passwordManagerDriver:(password_manager::PasswordManagerDriver*)driver
        JSPasswordManager:(JsPasswordManager*)JSPasswordManager
      JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager
      passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate;

// Clears all per-page state.
- (void)clearState;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_AGENT_H_
