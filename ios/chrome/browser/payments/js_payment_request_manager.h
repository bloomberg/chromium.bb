// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_

#include "base/strings/string16.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

namespace web {
class PaymentAddress;
class PaymentResponse;
class PaymentShippingOption;
}

// Injects the JavaScript that implements the Payment Request API and provides
// an app-side interface for interacting with it.
@interface JSPaymentRequestManager : CRWJSInjectionManager

// Executes a JS noop function. This is used to work around an issue where the
// JS event queue is blocked while presenting the Payment Request UI.
- (void)executeNoop;

// Resolves the JavaScript promise associated with the current PaymentRequest
// with the a JSON serialization of |paymentResponse|. If |completionHandler| is
// not nil, it will be invoked with YES after the operation has completed
// successfully or with NO otherwise.
- (void)resolveRequestPromiseWithPaymentResponse:
            (const web::PaymentResponse&)paymentResponse
                               completionHandler:
                                   (ProceduralBlockWithBool)completionHandler;

// Rejects the JavaScript promise associated with the current PaymentRequest
// with the supplied |errorMessage|. If |completionHandler| is not nil, it will
// be invoked with YES after the operation has completed successfully or with NO
// otherwise.
- (void)rejectRequestPromiseWithErrorMessage:(NSString*)errorMessage
                           completionHandler:
                               (ProceduralBlockWithBool)completionHandler;

// Resolves the promise returned by PaymentRequest.prototype.abort.
- (void)resolveAbortPromiseWithCompletionHandler:
    (ProceduralBlockWithBool)completionHandler;

// Resolves the JavaScript promise associated with the current PaymentResponse.
// If |completionHandler| is not nil, it will be invoked with YES after the
// operation has completed successfully or with NO otherwise.
- (void)resolveResponsePromiseWithCompletionHandler:
    (ProceduralBlockWithBool)completionHandler;

// Updates the shippingAddress property on the PaymentRequest object and
// dispatches a shippingaddresschange event.
- (void)updateShippingAddress:(const web::PaymentAddress&)shippingAddress
            completionHandler:(ProceduralBlockWithBool)completionHanlder;

// Updates the shippingOption property on the PaymentRequest object and
// dispatches a shippingoptionchange event.
- (void)updateShippingOption:(const web::PaymentShippingOption&)shippingOption
           completionHandler:(ProceduralBlockWithBool)completionHanlder;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_
