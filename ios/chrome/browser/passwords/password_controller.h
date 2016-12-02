// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_

#import <Foundation/NSObject.h>

#include <memory>

#import "ios/chrome/browser/autofill/form_suggestion_provider.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_driver.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@protocol FormInputAccessoryViewProvider;
@protocol PasswordsUiDelegate;
@class PasswordGenerationAgent;

namespace password_manager {
class PasswordGenerationManager;
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

// Per-tab password controller. Handles password autofill and saving.
@interface PasswordController : NSObject<CRWWebStateObserver,
                                         PasswordManagerClientDelegate,
                                         PasswordManagerDriverDelegate>

// An object that can provide suggestions from this PasswordController.
@property(readonly) id<FormSuggestionProvider> suggestionProvider;

// An object that can provide an input accessory view from this
// PasswordController.
@property(readonly) id<FormInputAccessoryViewProvider> accessoryViewProvider;

// The PasswordGenerationManager owned by this PasswordController.
@property(readonly)
    password_manager::PasswordGenerationManager* passwordGenerationManager;

// The PasswordManagerClient owned by this PasswordController.
@property(readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

// The PasswordManagerDriver owned by this PasswordController.
@property(readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// |webState| should not be nil.
- (instancetype)initWithWebState:(web::WebState*)webState
             passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate;

// This is just for testing.
- (instancetype)
   initWithWebState:(web::WebState*)webState
passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate
             client:(std::unique_ptr<password_manager::PasswordManagerClient>)
                        passwordManagerClient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Releases all tab-specific members. Must be called when the Tab is closing,
// otherwise invalid memory might be accessed during destruction.
- (void)detach;

// Uses JavaScript to find password forms using the |webState_| and fills
// them with the |username| and |password|. |completionHandler|, if not nil,
// is called once per form filled.
- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
