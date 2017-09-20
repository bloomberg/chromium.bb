// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HTTPAuthDialogRequest ()

// The authentication callback provided by WebKit.
@property(nonatomic, copy) HTTPAuthDialogCallback callback;

// Whether |callback| has been executed.
@property(nonatomic, assign) BOOL callbackHasBeenExecuted;

// Private initializer used by factory method.
- (instancetype)initWithWebState:(web::WebState*)webState
                 protectionSpace:(NSURLProtectionSpace*)protectionSpace
                      credential:(NSURLCredential*)credential
                        callback:(HTTPAuthDialogCallback)callback
    NS_DESIGNATED_INITIALIZER;

@end

@implementation HTTPAuthDialogRequest
@synthesize webState = _webState;
@synthesize title = _title;
@synthesize message = _message;
@synthesize defaultUserNameText = _defaultUserNameText;
@synthesize callback = _callback;
@synthesize callbackHasBeenExecuted = _callbackHasBeenExecuted;

- (instancetype)initWithWebState:(web::WebState*)webState
                 protectionSpace:(NSURLProtectionSpace*)protectionSpace
                      credential:(NSURLCredential*)credential
                        callback:(HTTPAuthDialogCallback)callback {
  DCHECK(webState);
  DCHECK(protectionSpace);
  DCHECK(credential);
  DCHECK(callback);
  if ((self = [super init])) {
    _webState = webState;
    _title = l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE);
    _message = nsurlprotectionspace_util::MessageForHTTPAuth(protectionSpace);
    _defaultUserNameText = credential.user ? credential.user : @"";
    _callback = [callback copy];
  }
  return self;
}

- (void)dealloc {
  DCHECK(_callbackHasBeenExecuted);
}

#pragma mark - Public

+ (instancetype)requestWithWebState:(web::WebState*)webState
                    protectionSpace:(NSURLProtectionSpace*)protectionSpace
                         credential:(NSURLCredential*)credential
                           callback:(HTTPAuthDialogCallback)callback {
  return [[self alloc] initWithWebState:webState
                        protectionSpace:protectionSpace
                             credential:credential
                               callback:callback];
}

- (void)completeAuthenticationWithUsername:(NSString*)username
                                  password:(NSString*)password {
  if (self.callbackHasBeenExecuted)
    return;
  self.callback(username, password);
  self.callbackHasBeenExecuted = YES;
}

@end
