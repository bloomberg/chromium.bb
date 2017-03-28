// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// TODO(crbug.com/704946): Make framework style include work everywhere and
// remove this #if.
#if defined(CWV_IMPLEMENTATION)
#include "ios/web_view/public/cwv_export.h"
#else
#include <ChromeWebView/cwv_export.h>
#endif

@protocol CWVDelegate;
@class CWVWebView;

// Main interface for the CWV library.
CWV_EXPORT
@interface CWV : NSObject

// Initializes the CWV library. This function should be called from
// |application:didFinishLaunchingWithOptions:|.
+ (void)configureWithDelegate:(id<CWVDelegate>)delegate;

// Shuts down the CWV library.  This function should be called from
// |applicationwillTerminate:|.
+ (void)shutDown;

// Creates and returns a web view.
+ (CWVWebView*)webViewWithFrame:(CGRect)frame;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_H_
