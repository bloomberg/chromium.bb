// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

@class CRWPendingNavigationInfo;
@class CRWWKNavigationStates;

// CRWWKNavigationHandler uses this protocol to interact with its owner.
@protocol CRWWKNavigationHandlerDelegate <NSObject>

@end

// Handler class for WKNavigationDelegate, deals with navigation callbacks from
// WKWebView and maintains page loading state.
@interface CRWWKNavigationHandler : NSObject <WKNavigationDelegate>

// Pending information for an in-progress page navigation. The lifetime of
// this object starts at |decidePolicyForNavigationAction| where the info is
// extracted from the request, and ends at either |didCommitNavigation| or
// |didFailProvisionalNavigation|.
@property(nonatomic, strong) CRWPendingNavigationInfo* pendingNavigationInfo;

// Holds all WKNavigation objects and their states which are currently in
// flight.
@property(nonatomic, readonly, strong) CRWWKNavigationStates* navigationStates;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
