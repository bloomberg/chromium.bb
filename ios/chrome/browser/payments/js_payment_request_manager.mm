// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/js_payment_request_manager.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sanitizes |JSON| and wraps it in quotes so it can be injected safely in
// JavaScript.
NSString* JSONEscape(NSString* JSON) {
  return base::SysUTF8ToNSString(
      base::GetQuotedJSONString(base::SysNSStringToUTF8(JSON)));
}

}  // namespace

@interface JSPaymentRequestManager ()

// Executes the JavaScript in |script| and passes a BOOL to |completionHandler|
// indicating whether an error occurred. The resolve and reject functions in the
// Payment Request JavaScript do not return values so the result is ignored.
- (void)executeScript:(NSString*)script
    completionHandler:(ProceduralBlockWithBool)completionHandler;

@end

@implementation JSPaymentRequestManager

- (void)executeNoop {
  [self executeScript:@"Function.prototype()" completionHandler:nil];
}

- (void)resolveRequestPromiseWithPaymentResponse:
            (const web::PaymentResponse&)paymentResponse
                               completionHandler:
                                   (ProceduralBlockWithBool)completionHandler {
  std::unique_ptr<base::DictionaryValue> paymentResponseData =
      paymentResponse.ToDictionaryValue();
  std::string paymentResponseDataJSON;
  base::JSONWriter::Write(*paymentResponseData, &paymentResponseDataJSON);
  NSString* script = [NSString
      stringWithFormat:
          @"__gCrWeb['paymentRequestManager'].resolveRequestPromise(%@)",
          base::SysUTF8ToNSString(paymentResponseDataJSON)];
  [self executeScript:script completionHandler:completionHandler];
}

- (void)rejectRequestPromiseWithErrorMessage:(NSString*)errorMessage
                           completionHandler:
                               (ProceduralBlockWithBool)completionHandler {
  NSString* script = [NSString
      stringWithFormat:
          @"__gCrWeb['paymentRequestManager'].rejectRequestPromise(%@)",
          JSONEscape(errorMessage)];
  [self executeScript:script completionHandler:completionHandler];
}

- (void)resolveAbortPromiseWithCompletionHandler:
    (ProceduralBlockWithBool)completionHandler {
  NSString* script = @"__gCrWeb['paymentRequestManager'].resolveAbortPromise()";
  [self executeScript:script completionHandler:completionHandler];
}

- (void)resolveResponsePromiseWithCompletionHandler:
    (ProceduralBlockWithBool)completionHandler {
  NSString* script =
      @"__gCrWeb['paymentRequestManager'].resolveResponsePromise()";
  [self executeScript:script completionHandler:completionHandler];
}

- (void)updateShippingAddress:(const web::PaymentAddress&)shippingAddress
            completionHandler:(ProceduralBlockWithBool)completionHanlder {
  std::unique_ptr<base::DictionaryValue> shippingAddressData =
      shippingAddress.ToDictionaryValue();
  std::string shippingAddressDataJSON;
  base::JSONWriter::Write(*shippingAddressData, &shippingAddressDataJSON);
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb['paymentRequestManager']."
                       @"updateShippingAddressAndDispatchEvent(%@)",
                       base::SysUTF8ToNSString(shippingAddressDataJSON)];
  [self executeScript:script completionHandler:completionHanlder];
}

- (void)updateShippingOption:(const web::PaymentShippingOption&)shippingOption
           completionHandler:(ProceduralBlockWithBool)completionHanlder {
  NSString* script =
      [NSString stringWithFormat:@"__gCrWeb['paymentRequestManager']."
                                 @"updateShippingOptionAndDispatchEvent('%@')",
                                 base::SysUTF16ToNSString(shippingOption.id)];
  [self executeScript:script completionHandler:completionHanlder];
}

- (void)executeScript:(NSString*)script
    completionHandler:(ProceduralBlockWithBool)completionHandler {
  [self executeJavaScript:script
        completionHandler:^(id result, NSError* error) {
          if (completionHandler)
            completionHandler(!error);
        }];
}

#pragma mark - Protected methods

- (NSString*)scriptPath {
  return @"payment_request_manager";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.paymentRequestManager";
}

@end
