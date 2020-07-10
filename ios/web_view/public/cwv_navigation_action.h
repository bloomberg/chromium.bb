// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Encapsulates information about an action which caused a navigation.
CWV_EXPORT
@interface CWVNavigationAction : NSObject

// Destination request associated with the action.
@property(nonatomic, copy, readonly) NSURLRequest* request;
// YES if the action was caused by a user action (e.g. link tap).
@property(nonatomic, readonly, getter=isUserInitiated) BOOL userInitiated;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_
