// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@protocol ApplicationCommands;
@protocol FormInputAccessoryViewProvider;
@protocol FormSuggestionProvider;
@class PasswordController;
@protocol PasswordControllerDelegate;
@protocol PasswordFormFiller;
@protocol PasswordsUiDelegate;

namespace password_manager {
class PasswordGenerationManager;
}

// Class binding a PasswordController to a WebState.
class PasswordTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<PasswordTabHelper> {
 public:
  ~PasswordTabHelper() override;

  // Creates a PasswordTabHelper and attaches it to the given |web_state|.
  // |password_ui_delegate| may be nil.
  static void CreateForWebState(web::WebState* web_state,
                                id<PasswordsUiDelegate> passwords_ui_delegate);

  // Sets the PasswordController dispatcher.
  void SetDispatcher(id<ApplicationCommands> dispatcher);

  // Sets the PasswordController delegate.
  void SetPasswordControllerDelegate(id<PasswordControllerDelegate> delegate);

  // Returns an object that can provide suggestions from the PasswordController.
  // May return nil.
  id<FormSuggestionProvider> GetSuggestionProvider();

  // Returns an object that can provide an input accessory view from the
  // PasswordController.
  id<FormInputAccessoryViewProvider> GetAccessoryViewProvider();

  // Returns the PasswordFormFiller from the PasswordController.
  id<PasswordFormFiller> GetPasswordFormFiller();

  // Returns the PasswordGenerationManager owned by the PasswordController.
  password_manager::PasswordGenerationManager* GetPasswordGenerationManager();

 private:
  PasswordTabHelper(web::WebState* web_state,
                    id<PasswordsUiDelegate> passwords_ui_delegate);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The Objective-C password controller instance.
  __strong PasswordController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordTabHelper);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_
