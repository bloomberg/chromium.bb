// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_REQUEST_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_REQUEST_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// Block type for HTTP authentication callbacks.
typedef void (^HTTPAuthDialogCallback)(NSString* _Nullable username,
                                       NSString* _Nullable password);

// A container object encapsulating all the state necessary to support an
// HTTPAuthDialogCoordiantor.  This object also owns the WebKit
// completion block that will throw an exception if it is deallocated before
// being executed. |-runCallbackWithUserName:password:| must be executed once in
// the lifetime of every HTTPAuthDialogRequest.
@interface HTTPAuthDialogRequest : NSObject

// Factory method to create HTTPAuthDialogRequests from the given input.  All
// arguments to this function are expected to be non nil/null.
+ (nullable instancetype)
requestWithWebState:(nonnull web::WebState*)webState
    protectionSpace:(nonnull NSURLProtectionSpace*)protectionSpace
         credential:(nonnull NSURLCredential*)credential
           callback:(nonnull HTTPAuthDialogCallback)callback;
- (nullable instancetype)init NS_UNAVAILABLE;

// The WebState displaying this dialog.
@property(nonatomic, readonly, nonnull) web::WebState* webState;

// The title to use for the dialog.
@property(nonatomic, readonly, strong, nonnull) NSString* title;

// The authentication message for the dialog's protection space.
@property(nonatomic, readonly, strong, nonnull) NSString* message;

// The default text to display in the username text field.
@property(nonatomic, readonly, strong, nonnull) NSString* defaultUserNameText;

// Completes the HTTP authentication flow with the given username and password.
// If the user taps the OK button, |username| and |password| are expected to be
// non-nil, even if they are empty strings.  If the user taps the Cancel button,
// arguments should be nil.
- (void)completeAuthenticationWithUsername:(nullable NSString*)username
                                  password:(nullable NSString*)password;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_REQUEST_H_
