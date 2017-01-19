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

// Cancels any outstanding dialogs requested by the methods above.
- (void)cancelDialogsForWebController:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_
