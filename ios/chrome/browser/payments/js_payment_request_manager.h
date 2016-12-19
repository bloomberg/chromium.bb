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

// Rejects the JavaScript promise associated with the current PaymentResponse
// with the supplied |errorMessage|. |completionHandler| will be invoked after
// the operation has completed with YES if successful.
- (void)rejectResponsePromise:(NSString*)errorMessage
            completionHandler:(void (^)(BOOL))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_JS_PAYMENT_REQUEST_MANAGER_H_
