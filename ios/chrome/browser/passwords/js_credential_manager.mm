// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/js_credential_manager.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/web_state/js/credential_util.h"

namespace {

// Sanitizes |JSON| and wraps it in quotes so it can be injected safely in
// JavaScript.
NSString* JSONEscape(NSString* JSON) {
  return base::SysUTF8ToNSString(
      base::GetQuotedJSONString(base::SysNSStringToUTF8(JSON)));
}

}  // namespace

const char kCredentialsPendingRequestErrorType[] = "PendingRequestError";
const char kCredentialsPendingRequestErrorMessage[] =
    "There is already an outstanding request";
const char kCredentialsSecurityErrorType[] = "SecurityError";
const char kCredentialsPasswordStoreUnavailableErrorType[] =
    "PasswordStoreUnavailableError";
const char kCredentialsPasswordStoreUnavailableErrorMessage[] =
    "The password store is unavailable";
const char kCredentialsSecurityErrorMessageUntrustedOrigin[] =
    "The origin is untrusted";

@interface JSCredentialManager ()

// Evaluates the JavaScript in |script|, which should evaluate to a JavaScript
// boolean value. That value will be passed to |completionHandler|.
- (void)evaluateScript:(NSString*)script
     completionHandler:(void (^)(BOOL))completionHandler;

@end

@implementation JSCredentialManager

- (void)resolvePromiseWithRequestID:(NSInteger)requestID
                         credential:(const web::Credential&)credential
                  completionHandler:(void (^)(BOOL))completionHandler {
  base::DictionaryValue credentialData;
  web::CredentialToDictionaryValue(credential, &credentialData);
  std::string credentialDataJSON;
  base::JSONWriter::Write(credentialData, &credentialDataJSON);
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb['credentialManager'].resolve(%ld, %@)",
                       static_cast<long>(requestID),
                       base::SysUTF8ToNSString(credentialDataJSON)];
  [self evaluate:script
      stringResultHandler:^(NSString* result, NSError* error) {
        if (completionHandler)
          completionHandler(!error && [result isEqualToString:@"true"]);
      }];
}

- (void)resolvePromiseWithRequestID:(NSInteger)requestID
                  completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script =
      [NSString stringWithFormat:@"__gCrWeb['credentialManager'].resolve(%ld)",
                                 static_cast<long>(requestID)];
  [self evaluateScript:script completionHandler:completionHandler];
}

- (void)rejectPromiseWithRequestID:(NSInteger)requestID
                         errorType:(NSString*)errorType
                           message:(NSString*)message
                 completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb['credentialManager'].reject(%ld, %@, %@)",
                       static_cast<long>(requestID), JSONEscape(errorType),
                       JSONEscape(message)];
  [self evaluateScript:script completionHandler:completionHandler];
}

- (void)evaluateScript:(NSString*)script
     completionHandler:(void (^)(BOOL))completionHandler {
  [self evaluate:script
      stringResultHandler:^(NSString* result, NSError* error) {
        if (completionHandler)
          completionHandler(!error && [result isEqualToString:@"true"]);
      }];
}

#pragma mark - Protected methods

- (NSString*)scriptPath {
  return @"credential_manager";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.credentialManager";
}

@end
