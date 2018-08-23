// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_controller_helper.h"

#include <stddef.h>

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/ios/js_password_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordForm;
using password_manager::GetPageURLAndCheckTrustLevel;

namespace password_manager {
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state,
                                  GURL* __nullable page_url) {
  auto trustLevel = web::URLVerificationTrustLevel::kNone;
  GURL dummy;
  if (!page_url) {
    page_url = &dummy;
  }
  *page_url = web_state->GetCurrentURL(&trustLevel);
  return trustLevel == web::URLVerificationTrustLevel::kAbsolute;
}
}  // namespace password_manager

namespace {
// Script command prefix for form changes. Possible command to be sent from
// injected JS is 'passwordForm.submitButtonClick'.
constexpr char kCommandPrefix[] = "passwordForm";
}  // namespace

@interface PasswordControllerHelper ()

@property(nonatomic, weak) id<PasswordControllerHelperDelegate> delegate;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

// Finds the currently submitted password form named |formName| and calls
// |completionHandler| with the populated data structure. |found| is YES if the
// current form was found successfully, NO otherwise. |completionHandler|
// cannot be nil.
- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const PasswordForm& form))completionHandler;

@end

@implementation PasswordControllerHelper {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;
}

#pragma mark - Properties

@synthesize delegate = _delegate;
@synthesize jsPasswordManager = _jsPasswordManager;

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:
                            (id<PasswordControllerHelperDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _delegate = delegate;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
    _jsPasswordManager = [[JsPasswordManager alloc]
        initWithReceiver:_webState->GetJSInjectionReceiver()];

    __weak PasswordControllerHelper* weakSelf = self;
    auto callback = base::BindRepeating(
        ^bool(const base::DictionaryValue& JSON, const GURL& originURL,
              bool interacting, bool isMainFrame) {
          if (!isMainFrame) {
            // Passwords is only supported on main frame.
            return false;
          }
          // |originURL| and |interacting| aren't used.
          return [weakSelf handleScriptCommand:JSON];
        });
    _webState->AddScriptCommandCallback(callback, kCommandPrefix);
  }
  return self;
}

#pragma mark - Dealloc

- (void)dealloc {
  if (_webState) {
    _webState->RemoveScriptCommandCallback(kCommandPrefix);
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveScriptCommandCallback(kCommandPrefix);
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  _formActivityObserverBridge.reset();
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    submittedDocumentWithFormNamed:(const std::string&)formName
                    hasUserGesture:(BOOL)hasUserGesture
                   formInMainFrame:(BOOL)formInMainFrame {
  DCHECK_EQ(_webState, webState);
  __weak PasswordControllerHelper* weakSelf = self;
  // This code is racing against the new page loading and will not get the
  // password form data if the page has changed. In most cases this code wins
  // the race.
  // TODO(crbug.com/418827): Fix this by passing in more data from the JS side.
  id completionHandler = ^(BOOL found, const autofill::PasswordForm& form) {
    PasswordControllerHelper* strongSelf = weakSelf;
    id<PasswordControllerHelperDelegate> strongDelegate = strongSelf.delegate;
    if (!strongSelf || !strongSelf->_webState || !strongDelegate) {
      return;
    }
    [strongDelegate helper:strongSelf
             didSubmitForm:form
               inMainFrame:formInMainFrame];
  };
  [self extractSubmittedPasswordForm:formName
                   completionHandler:completionHandler];
}

#pragma mark - Private methods

- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand {
  std::string command;
  if (!JSONCommand.GetString("command", &command)) {
    return NO;
  }

  if (command != "passwordForm.submitButtonClick") {
    return NO;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    return NO;
  }

  FormData formData;
  if (!autofill::ExtractFormData(JSONCommand, false, base::string16(), pageURL,
                                 &formData)) {
    return NO;
  }

  std::unique_ptr<PasswordForm> form =
      ParseFormData(formData, password_manager::FormParsingMode::SAVING);
  if (!form) {
    return NO;
  }

  if (_webState && self.delegate) {
    [self.delegate helper:self didSubmitForm:*form inMainFrame:YES];
    return YES;
  }

  return NO;
}

- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const PasswordForm& form))completionHandler {
  DCHECK(completionHandler);

  if (!_webState) {
    return;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    completionHandler(NO, PasswordForm());
    return;
  }

  id extractSubmittedFormCompletionHandler = ^(NSString* jsonString) {
    std::unique_ptr<base::Value> formValue = autofill::ParseJson(jsonString);
    if (!formValue) {
      completionHandler(NO, PasswordForm());
      return;
    }

    FormData formData;
    if (!autofill::ExtractFormData(*formValue, false, base::string16(), pageURL,
                                   &formData)) {
      completionHandler(NO, PasswordForm());
      return;
    }

    std::unique_ptr<PasswordForm> form =
        ParseFormData(formData, password_manager::FormParsingMode::SAVING);
    if (!form) {
      completionHandler(NO, PasswordForm());
      return;
    }

    completionHandler(YES, *form);
  };

  [self.jsPasswordManager extractForm:base::SysUTF8ToNSString(formName)
                    completionHandler:extractSubmittedFormCompletionHandler];
}

@end
