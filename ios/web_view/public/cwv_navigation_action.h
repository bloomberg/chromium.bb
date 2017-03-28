// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_

#import <Foundation/Foundation.h>

// TODO(crbug.com/704946): Make framework style include work everywhere and
// remove this #if.
#if defined(CWV_IMPLEMENTATION)
#include "ios/web_view/public/cwv_export.h"
#else
#include <ChromeWebView/cwv_export.h>
#endif

// Encapsulates information about an action which caused a navigation.
CWV_EXPORT
@interface CWVNavigationAction : NSObject

// Destination request associated with the action.
@property(nonatomic, copy, readonly, nonnull) NSURLRequest* request;
// YES if the action was caused by a user action (e.g. link tap).
@property(nonatomic, readonly, getter=isUserInitiated) BOOL userInitiated;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_ACTION_H_
