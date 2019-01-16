// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_

#import <Foundation/NSObject.h>

#include <memory>

#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_driver.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@protocol ApplicationCommands;
@class NotifyUserAutoSigninViewController;
@protocol PasswordFormFiller;
@protocol PasswordsUiDelegate;
@class UIViewController;

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

// Delegate for registering view controller and displaying its view. Used to
// add views to BVC.
@protocol PasswordControllerDelegate

// Adds |viewController| as child controller in order to display auto sign-in
// notification. Returns YES if view was displayed, NO otherwise.
- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId;

// Opens the list of saved passwords in the settings.
- (void)displaySavedPasswordList;

@end

// Per-tab password controller. Handles password autofill and saving.
@interface PasswordController : NSObject<CRWWebStateObserver,
                                         PasswordManagerClientDelegate,
                                         PasswordManagerDriverDelegate,
                                         PasswordFormHelperDelegate>

// An object that can provide suggestions from this PasswordController.
@property(nonatomic, readonly) id<FormSuggestionProvider> suggestionProvider;

// The PasswordManagerClient owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

// The PasswordManagerDriver owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// The PasswordFormFiller owned by this PasswordController.
@property(nonatomic, readonly) id<PasswordFormFiller> passwordFormFiller;

// The base view controller from which to present UI.
@property(nonatomic, readwrite, weak) UIViewController* baseViewController;

// The dispatcher used for the PasswordController. This property can return nil
// even after being set to a non-nil object.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Delegate used by this PasswordController to show UI on BVC.
@property(weak, nonatomic) id<PasswordControllerDelegate> delegate;

// |webState| should not be nil.
- (instancetype)initWithWebState:(web::WebState*)webState;

// This is just for testing.
- (instancetype)
   initWithWebState:(web::WebState*)webState
             client:(std::unique_ptr<password_manager::PasswordManagerClient>)
                        passwordManagerClient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Generates and offers a password to the user based on given |formName| for the
// given (optional) fields |newPasswordIdentfier| and
// |confirmPasswordIdentfier|. |completionHandler| called with YES if user
// accepted the generated password.
- (void)generatePasswordForFormName:(NSString*)formName
              newPasswordIdentifier:(NSString*)newPasswordIdentifier
          confirmPasswordIdentifier:(NSString*)confirmPasswordIdentifier
                  completionHandler:(void (^)(BOOL))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
