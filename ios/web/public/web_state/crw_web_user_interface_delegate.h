// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CRWWebController;

// DEPRECATED, do not conform to this protocol and do not add any methods to it.
// Use web::WebStateDelegate instead.
// TODO(crbug.com/675001): Remove this protocol.
@protocol CRWWebUserInterfaceDelegate<NSObject>

 @optional

// Displays an HTTP authentication dialog.  |completionHandler| should be called
// with non-nil |username| and |password| if embedder wants to proceed with
// authentication.  If this selector isn't implemented or completion is called
// with nil |username| and nil |password|, authentication will be cancelled.
// If this method is implemented, but |handler| is not called then
// NSInternalInconsistencyException will be thrown.
- (void)webController:(CRWWebController*)webController
    runAuthDialogForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                 proposedCredential:(NSURLCredential*)credential
                  completionHandler:
                      (void (^)(NSString* user, NSString* password))handler;

// Cancels any outstanding dialogs requested by the methods above.
- (void)cancelDialogsForWebController:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_
