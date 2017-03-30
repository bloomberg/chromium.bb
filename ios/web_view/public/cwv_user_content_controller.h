// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

@class CWVUserScript;

// Allows injecting custom scripts into CWVWebView created with the
// configuration.
CWV_EXPORT
@interface CWVUserContentController : NSObject

// The user scripts associated with the configuration.
@property(nonatomic, copy, readonly, nonnull)
    NSArray<CWVUserScript*>* userScripts;

- (nonnull instancetype)init NS_UNAVAILABLE;

// Adds a user script.
- (void)addUserScript:(nonnull CWVUserScript*)userScript;

// Removes all associated user scripts.
- (void)removeAllUserScripts;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
