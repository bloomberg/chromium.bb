// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import <memory>

#include "ui/base/page_transition_types.h"

@class CRWWKNavigationHandler;
@class CRWPendingNavigationInfo;
@class CRWWKNavigationStates;
class GURL;
namespace base {
class RepeatingTimer;
}
namespace web {
enum class WKNavigationState;
struct Referrer;
class WebStateImpl;
class NavigationContextImpl;
class UserInteractionState;
}

// CRWWKNavigationHandler uses this protocol to interact with its owner.
@protocol CRWWKNavigationHandlerDelegate <NSObject>

// Returns associated WebStateImpl.
- (web::WebStateImpl*)webStateImplForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns associated UserInteractionState.
- (web::UserInteractionState*)userInteractionStateForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns current Referrer.
- (web::Referrer)currentReferrerForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns YES if WKWebView is halted.
- (BOOL)navigationHandlerWebViewIsHalted:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns YES if WKWebView was deallocated or is being deallocated.
- (BOOL)navigationHandlerWebViewBeingDestroyed:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns the actual URL of the document object (i.e., the last committed URL
// of the main frame).
- (GURL)navigationHandlerDocumentURL:(CRWWKNavigationHandler*)navigationHandler;

// Sets document URL to newURL, and updates any relevant state information.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
           setDocumentURL:(const GURL&)newURL
                  context:(web::NavigationContextImpl*)context;

// Maps WKNavigationType to ui::PageTransition.
- (ui::PageTransition)navigationHandler:
                          (CRWWKNavigationHandler*)navigationHandler
       pageTransitionFromNavigationType:(WKNavigationType)navigationType;

// Sets up WebUI for URL.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
        createWebUIForURL:(const GURL&)URL;

// Stop Loading current page.
- (void)navigationHandlerStopLoading:(CRWWKNavigationHandler*)navigationHandler;

// Aborts any load for both the web view and its controller.
- (void)navigationHandlerAbortLoading:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns YES if |url| should be loaded in a native view.
- (BOOL)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    shouldLoadURLInNativeView:(const GURL&)url;

// Requires that the next load rebuild the web view. This is expensive, and
// should be used only in the case where something has changed that the web view
// only checks on creation, such that the whole object needs to be rebuilt.
- (void)navigationHandlerRequirePageReconstruction:
    (CRWWKNavigationHandler*)navigationHandler;

- (std::unique_ptr<web::NavigationContextImpl>)
            navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)renderedInitiated
        placeholderNavigation:(BOOL)placeholderNavigation;

@end

// Handler class for WKNavigationDelegate, deals with navigation callbacks from
// WKWebView and maintains page loading state.
@interface CRWWKNavigationHandler : NSObject <WKNavigationDelegate>

@property(nonatomic, weak) id<CRWWKNavigationHandlerDelegate> delegate;

// TODO(crbug.com/956511): Change this to readonly when
// |webViewWebProcessDidCrash| is moved to CRWWKNavigationHandler.
@property(nonatomic, assign) BOOL webProcessCrashed;

// Pending information for an in-progress page navigation. The lifetime of
// this object starts at |decidePolicyForNavigationAction| where the info is
// extracted from the request, and ends at either |didCommitNavigation| or
// |didFailProvisionalNavigation|.
@property(nonatomic, strong) CRWPendingNavigationInfo* pendingNavigationInfo;

// Holds all WKNavigation objects and their states which are currently in
// flight.
@property(nonatomic, readonly, strong) CRWWKNavigationStates* navigationStates;

// The current page loading phase.
// TODO(crbug.com/956511): Remove this once refactor is done.
@property(nonatomic, readwrite, assign) web::WKNavigationState navigationState;

// The SafeBrowsingDetection timer.
// TODO(crbug.com/956511): Remove this once refactor is done.
@property(nonatomic, readonly, assign)
    base::RepeatingTimer* safeBrowsingWarningDetectionTimer;

// Discards non committed items, only if the last committed URL was not loaded
// in native view. But if it was a native view, no discard will happen to avoid
// an ugly animation where the web view is inserted and quickly removed.
- (void)discardNonCommittedItemsIfLastCommittedWasNotNativeView;

// Instructs this handler to stop loading.
- (void)stopLoading;

// Informs this handler that any outstanding load operations are cancelled.
- (void)loadCancelled;

// Returns context for pending navigation that has |URL|. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL;

// Notifies that server redirect has been received.
// TODO(crbug.com/956511): Remove this once "webView:didCommitNavigation" is
// moved into CRWWKNavigationHandler.
- (void)didReceiveRedirectForNavigation:(web::NavigationContextImpl*)context
                                withURL:(const GURL&)URL;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
