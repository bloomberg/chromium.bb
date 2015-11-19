// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_JS_CREDENTIAL_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_JS_CREDENTIAL_MANAGER_H_

#include "ios/web/public/web_state/credential.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

namespace base {
class DictionaryValue;
}  // namespace base

// Constants for rejecting requests.
extern const char kCredentialsPendingRequestErrorType[];
extern const char kCredentialsPendingRequestErrorMessage[];
extern const char kCredentialsSecurityErrorType[];
extern const char kCredentialsPasswordStoreUnavailableErrorType[];
extern const char kCredentialsPasswordStoreUnavailableErrorMessage[];
extern const char kCredentialsSecurityErrorMessageUntrustedOrigin[];

// Injects the JavaScript that implements the request credentials API and
// provides an app-side interface for interacting with it.
@interface JSCredentialManager : CRWJSInjectionManager

// Resolves the JavaScript Promise associated with |requestID| with the
// specified |credential|. |completionHandler| will be invoked after the
// operation has completed with YES if successful.
- (void)resolvePromiseWithRequestID:(NSInteger)requestID
                         credential:(const web::Credential&)credential
                  completionHandler:(void (^)(BOOL))completionHandler;

// Resolves the JavaScript Promise associated with |requestID|.
// |completionHandler| will be invoked after the operation has completed with
// YES if successful.
- (void)resolvePromiseWithRequestID:(NSInteger)requestID
                  completionHandler:(void (^)(BOOL))completionHandler;

// Rejects the JavaScript Promise associated with |requestID| with an Error of
// the specified |errorType| and |message|. |completionHandler| will be invoked
// after the operation has completed with YES if successful.
- (void)rejectPromiseWithRequestID:(NSInteger)requestID
                         errorType:(NSString*)errorType
                           message:(NSString*)message
                 completionHandler:(void (^)(BOOL))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_JS_CREDENTIAL_MANAGER_H_
