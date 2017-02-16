// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_MANAGER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_MANAGER_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// Manager for handling invocations of the Payment Request API.
//
// Implements the app-side of the Payment Request JavaScript API. Injects and
// listens to the injected JavaScript and invokes the creation of the user
// interface.
@interface PaymentRequestManager : NSObject

// YES if Payment Request is enabled on the current web state.
@property(readonly) BOOL enabled;

// The current web state being observed for PaymentRequest invocations.
@property(nonatomic, assign) web::WebState* webState;

// The ios::ChromeBrowserState instance passed to the initializer.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the WebState to be observed for invocations of the Payment Request API.
// |webState|'s lifetime should be greater than the receiver's. |webState| can
// be nil.
- (void)setWebState:(web::WebState*)webState;

// Enables or disables the Payment Request API for the current webState. If
// |enabled| is YES, the API may still not be enabled if the flag is not set;
// the -enabled property will indicate the current status. This method functions
// asynchronously.
- (void)enablePaymentRequest:(BOOL)enabled;

// Cancels the pending request and dismiss the UI.
- (void)cancelRequest;

// Destroys the receiver. Any following call is not supported.
- (void)close;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_MANAGER_H_
