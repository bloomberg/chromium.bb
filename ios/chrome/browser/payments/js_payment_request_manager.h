// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

namespace web {
class PaymentResponse;
}

// Injects the JavaScript that implements the Payment Request API and provides
// an app-side interface for interacting with it.
@interface JSPaymentRequestManager : CRWJSInjectionManager

// Executes a JS noop function. This is used to work around an issue where the
// JS event queue is blocked while presenting the Payment Request UI.
- (void)executeNoop;

// Resolves the JavaScript promise associated with the current PaymentRequest
// with the a JSON serialization of |paymentResponse|. |completionHandler| will
// be invoked after the operation has completed with YES if successful.
- (void)resolveRequestPromise:(const web::PaymentResponse&)paymentResponse
            completionHandler:(void (^)(BOOL))completionHandler;

// Rejects the JavaScript promise associated with the current PaymentRequest
// with the supplied |errorMessage|. |completionHandler| will be invoked after
// the operation has completed with YES if successful.
- (void)rejectRequestPromise:(NSString*)errorMessage
           completionHandler:(void (^)(BOOL))completionHandler;

// Resolves the JavaScript promise associated with the current PaymentResponse.
// |completionHandler| will be invoked after the operation has completed with
// YES if successful.
- (void)resolveResponsePromise:(void (^)(BOOL))completionHandler;

// Note that there is no rejectResponsePromise method because the spec includes
// no provision for rejecting the response promise. User agents are directed to
// always resolve the promise.

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_
