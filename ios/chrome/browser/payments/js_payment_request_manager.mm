// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/js_payment_request_manager.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/payments/payment_request.h"

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
    completionHandler:(void (^)(BOOL))completionHandler;

@end

@implementation JSPaymentRequestManager

- (void)resolveRequestPromise:(const web::PaymentResponse&)paymentResponse
            completionHandler:(void (^)(BOOL))completionHandler {
  std::unique_ptr<base::DictionaryValue> paymentResponseData =
      paymentResponse.ToDictionaryValue();
  std::string paymentResponseDataJSON;
  base::JSONWriter::Write(*paymentResponseData, &paymentResponseDataJSON);
  NSString* script = [NSString
      stringWithFormat:
          @"__gCrWeb['paymentRequestManager'].pendingRequest.resolve(%@)",
          base::SysUTF8ToNSString(paymentResponseDataJSON)];
  [self executeScript:script completionHandler:completionHandler];
}

- (void)rejectRequestPromise:(NSString*)errorMessage
           completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script = [NSString
      stringWithFormat:
          @"__gCrWeb['paymentRequestManager'].pendingRequest.reject(%@)",
          JSONEscape(errorMessage)];
  [self executeScript:script completionHandler:completionHandler];
}

- (void)resolveResponsePromise:(void (^)(BOOL))completionHandler {
  NSString* script =
      @"__gCrWeb['paymentRequestManager'].pendingResponse.resolve()";
  [self executeScript:script completionHandler:completionHandler];
}

- (void)rejectResponsePromise:(NSString*)errorMessage
            completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script = [NSString
      stringWithFormat:
          @"__gCrWeb['paymentRequestManager'].pendingResponse.reject(%@)",
          JSONEscape(errorMessage)];
  [self executeScript:script completionHandler:completionHandler];
}

- (void)executeScript:(NSString*)script
    completionHandler:(void (^)(BOOL))completionHandler {
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
