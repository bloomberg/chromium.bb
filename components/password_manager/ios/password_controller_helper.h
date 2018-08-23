// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_HELPER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "url/gurl.h"

NS_ASSUME_NONNULL_BEGIN

@class JsPasswordManager;
@class PasswordControllerHelper;

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {
// Returns true if the trust level for the current page URL of |web_state| is
// kAbsolute. If |page_url| is not null, fills it with the current page URL.
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state,
                                  GURL* __nullable page_url);
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

// A protocol implemented by a delegate of PasswordControllerHelper.
@protocol PasswordControllerHelperDelegate
// Called when the password form is submitted.
- (void)helper:(PasswordControllerHelper*)helper
    didSubmitForm:(const autofill::PasswordForm&)form
      inMainFrame:(BOOL)inMainFrame;
@end

// Handles common logic of password controller for both ios/chrome and
// ios/web_view.
// TODO(crbug.com/865114): Rename to PasswordFormHelper.
@interface PasswordControllerHelper
    : NSObject<FormActivityObserver, CRWWebStateObserver>

// The JsPasswordManager processing password form via javascript.
@property(nonatomic, readonly) JsPasswordManager* jsPasswordManager;

// Creates a instance with the given WebState, observer and delegate.
- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(nullable id<PasswordControllerHelperDelegate>)
                                     delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_HELPER_H_
