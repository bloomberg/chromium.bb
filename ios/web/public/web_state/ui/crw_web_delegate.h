// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_

#include <stdint.h>
#import <UIKit/UIKit.h>
#include <vector>

#import "base/ios/block_types.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class CRWWebController;

// Methods implemented by the delegate of the CRWWebController.
// DEPRECATED, do not conform to this protocol and do not add any methods to it.
// Use web::WebStateDelegate instead.
// TODO(crbug.com/674991): Remove this protocol.
@protocol CRWWebDelegate<NSObject>

// Called when an external app needs to be opened, it also passes |linkClicked|
// to track if this call was a result of user action or not. Returns YES iff
// |URL| is launched in an external app.
// |sourceURL| is the original URL that triggered the navigation to |URL|.
- (BOOL)openExternalURL:(const GURL&)URL
              sourceURL:(const GURL&)sourceURL
            linkClicked:(BOOL)linkClicked;

// This method is invoked whenever the system believes the URL is about to
// change, or immediately after any unexpected change of the URL, prior to
// updating the navigation manager's pending entry.
// Phase will be LOAD_REQUESTED.
- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition;
// Called when a placeholder image should be displayed instead of the WebView.
- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block;

// ---------------------------------------------------------------------
// TODO(rohitrao): Eliminate as many of the following delegate methods as
// possible.  They only exist because the Tab and CRWWebController logic was
// very intertwined. We should streamline the logic to jump between classes
// less, then remove any delegate method that becomes unnecessary as a result.

// Called when a page is loaded using loadWithParams.
- (void)webDidUpdateSessionForLoadWithParams:
            (const web::NavigationManager::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation;

@optional

// Called to ask CRWWebDelegate if |CRWWebController| should open the given URL.
// CRWWebDelegate can intercept the request by returning NO and processing URL
// in own way.
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked;

// Called to ask if external URL should be opened. External URL is one that
// cannot be presented by CRWWebController.
- (BOOL)webController:(CRWWebController*)webController
    shouldOpenExternalURL:(const GURL&)URL;

// Called to ask the delegate for a controller to display the given url,
// which contained content that the UIWebView couldn't display. Returns
// the native controller to display if the delegate can handle the url,
// or nil otherwise.
- (id<CRWNativeContent>)controllerForUnhandledContentAtURL:(const GURL&)url;

// Called to retrieve the height of any header that is overlaying on top of the
// web view. This can be used to implement, for e.g. a toolbar that changes
// height dynamically. Returning a non-zero height affects the visible frame
// shown by the CRWWebController. 0.0 is assumed if not implemented.
- (CGFloat)headerHeightForWebController:(CRWWebController*)webController;

// Called when a PassKit file is downloaded. |data| should be the data from a
// PassKit file, but this is not guaranteed, and the delegate is responsible for
// error handling non PassKit data using -[PKPass initWithData:error:]. If the
// download does not successfully complete, |data| will be nil.
- (void)webController:(CRWWebController*)webController
    didLoadPassKitObject:(NSData*)data;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
