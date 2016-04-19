// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_

#include <stdint.h>
#import <UIKit/UIKit.h>
#include <vector>

#include "base/ios/block_types.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class CRWSessionEntry;
@class CRWWebController;

namespace net {
class HttpResponseHeaders;
class SSLInfo;
class X509Certificate;
}

namespace web {
class BlockedPopupInfo;
struct Referrer;
}

// Callback for -presentSSLError:forSSLStatus:onUrl:recoverable:callback:
typedef void (^SSLErrorCallback)(BOOL);

// Methods implemented by the delegate of the CRWWebController.
@protocol CRWWebDelegate<NSObject>

// Called when the page wants to open a new window by DOM (e.g. with
// |window.open| JavaScript call or by clicking a link with |_blank| target) or
// wants to open a window with a new tab. |inBackground| allows a page to force
// a new window to open in the background. CRWSessionController's openedByDOM
// property of the returned CRWWebController must be YES.
- (CRWWebController*)webPageOrderedOpen:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             windowName:(NSString*)windowName
                           inBackground:(BOOL)inBackground;

// Called when the page wants to open a new window by DOM.
// CRWSessionController's openedByDOM property of the returned CRWWebController
// must be YES.
- (CRWWebController*)webPageOrderedOpen;

// Called when the page calls window.close() on itself. Begin the shut-down
// sequence for this controller.
- (void)webPageOrderedClose;
// Navigate forwards or backwards by delta pages.
- (void)goDelta:(int)delta;
// Opens a URL with the given parameters.
- (void)openURLWithParams:(const web::WebState::OpenURLParams&)params;
// Called when an external app needs to be opened, it also passes |linkClicked|
// to track if this call was a result of user action or not. Returns YES iff
// |URL| is launched in an external app.
- (BOOL)openExternalURL:(const GURL&)URL linkClicked:(BOOL)linkClicked;

// Asked the delegate to present an error to the user because the
// CRWWebController cannot verify the URL of the current page.
- (void)presentSpoofingError;
// This method is invoked whenever the system believes the URL is about to
// change, or immediately after any unexpected change of the URL, prior to
// updating the navigation manager's pending entry.
// Phase will be LOAD_REQUESTED.
- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition;
// This method is invoked after an update to the navigation manager's pending
// URL, triggered whenever the system believes the URL is about to
// change, or immediately after any unexpected change of the URL.
// This can be followed by a call to webDidStartLoading (phase PAGE_LOADING) or
// another call to webWillAddPendingURL and webDidAddPendingURL (phase still
// LOAD_REQUESTED).
- (void)webDidAddPendingURL;
// Called when webWillStartLoadingURL was called, but something went wrong, and
// webDidStartLoadingURL will now never be called.
- (void)webCancelStartLoadingRequest;
// Called when the page URL has changed. Phase will be PAGE_LOADING. Can be
// followed by webDidFinishWithURL or webWillStartLoadingURL.
// |updateHistory| is YES if the URL should be added to the history DB.
// TODO(stuartmorgan): Remove or rename the history param; the history DB
// isn't a web concept, so this shoud be expressed differently.
- (void)webDidStartLoadingURL:(const GURL&)url
          shouldUpdateHistory:(BOOL)updateHistory;
// Called when the page finishes loading, with the URL, page title and load
// success status. Phase will be PAGE_LOADED.
// On the next navigation event, this will be followed by a call to
// webWillStartLoadingURL.
- (void)webDidFinishWithURL:(const GURL&)url
                loadSuccess:(BOOL)loadSuccess;
// Called when the page load was cancelled by page activity (before a success /
// failure state is known). Phase will be PAGE_LOADED.
- (void)webLoadCancelled:(const GURL&)url;
// Called when a page updates its history stack using pushState or replaceState.
// TODO(stuartmorgan): Generalize this to cover any change of URL without page
// document change.
- (void)webDidUpdateHistoryStateWithPageURL:(const GURL&)pageUrl;
// Called when a placeholder image should be displayed instead of the WebView.
- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block;
// Consults the delegate whether a form should be resubmitted for a request.
// Occurs when a POST request is reached when navigating through history.
// Call |continueBlock| if a form should be resubmitted.
// Call |cancelBlock| if a form should not be resubmitted.
// Delegates must call either of these (just once) before the load will
// continue.
- (void)webController:(CRWWebController*)webController
    onFormResubmissionForRequest:(NSURLRequest*)request
                   continueBlock:(ProceduralBlock)continueBlock
                     cancelBlock:(ProceduralBlock)cancelBlock;
// Returns the unique id of the download request and starts downloading the
// image at |url| without sending the cookies. Invokes |callback| on completion.
- (int)downloadImageAtUrl:(const GURL&)url
    maxBitmapSize:(uint32_t)maxBitmapSize
         callback:(const web::WebState::ImageDownloadCallback&)callback;

// ---------------------------------------------------------------------
// TODO(rohitrao): Eliminate as many of the following delegate methods as
// possible.  They only exist because the Tab and CRWWebController logic was
// very intertwined. We should streamline the logic to jump between classes
// less, then remove any delegate method that becomes unneccessary as a result.

// Called when the page is reloaded.
- (void)webWillReload;
// Called when a page is loaded using loadWithParams.  In
// |webWillInitiateLoadWithParams|, the |params| argument is non-const so that
// the delegate can make changes if necessary.
// TODO(rohitrao): This is not a great API.  Clean it up.
- (void)webWillInitiateLoadWithParams:
    (web::NavigationManager::WebLoadParams&)params;
- (void)webDidUpdateSessionForLoadWithParams:
            (const web::NavigationManager::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation;
// Called from finishHistoryNavigationFromEntry.
- (void)webWillFinishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry;
// ---------------------------------------------------------------------

@optional

// This method is called when a network request has an issue with the SSL
// connection to present it to the user. The user will decide if the request
// should continue or not and the callback should be invoked to let the backend
// know.
// The callback is safe to call until CRWWebController is closed.
- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue;

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

// Called when |URL| is deemed suitable to be opened in a matching native app.
// Needs to return whether |URL| was opened in a matching native app.
// Also triggering user action |linkClicked| is passed to use it when needed.
// The return value indicates if the native app was launched, not if a native
// app was found.
// TODO(shreyasv): Instead of having the CRWWebDelegate handle an external URL,
// provide a hook/API to steal a URL navigation. That way the logic to determine
// a URL as triggering a native app launch can also be moved.
- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)URL
                         sourceURL:(const GURL&)sourceURL
                       linkClicked:(BOOL)linkClicked;

// Called to ask the delegate for a controller to display the given url,
// which contained content that the UIWebView couldn't display. Returns
// the native controller to display if the delegate can handle the url,
// or nil otherwise.
- (id<CRWNativeContent>)controllerForUnhandledContentAtURL:(const GURL&)url;

// Called when the page supplies a new title.
- (void)webController:(CRWWebController*)webController
       titleDidChange:(NSString*)title;

// Called when CRWWebController has detected a popup. If NO is returned then
// popup will be shown, otherwise |webController:didBlockPopup:| will be called
// and CRWWebDelegate will have a chance to unblock the popup later. NO is
// assumed by default if this method is not implemented.
- (BOOL)webController:(CRWWebController*)webController
    shouldBlockPopupWithURL:(const GURL&)popupURL
                  sourceURL:(const GURL&)sourceURL;

// Called when CRWWebController has detected and blocked a popup. In order to
// allow the blocked pop up CRWWebDelegate must call
// |blockedPopupInfo.ShowPopup()| instead of attempting to open a new window.
- (void)webController:(CRWWebController*)webController
        didBlockPopup:(const web::BlockedPopupInfo&)blockedPopupInfo;

// Called when CRWWebController did suppress a dialog (JavaScript, HTTP
// authentication or window.open).
// NOTE: Called only if CRWWebController.shouldSuppressDialogs is set to YES.
- (void)webControllerDidSuppressDialog:(CRWWebController*)webController;

// Called to retrieve the height of any header that is overlaying on top of the
// web view. This can be used to implement, for e.g. a toolbar that changes
// height dynamically. Returning a non-zero height affects the visible frame
// shown by the CRWWebController. 0.0 is assumed if not implemented.
- (CGFloat)headerHeightForWebController:(CRWWebController*)webController;

// Called when CRWWebController updated the SSL status for the current
// NagivationItem.
- (void)webControllerDidUpdateSSLStatusForCurrentNavigationItem:
    (CRWWebController*)webController;

// Called when CRWWebController did update page load progress.
- (void)webController:(CRWWebController*)webController
    didUpdateProgress:(CGFloat)progress;

// Called when web view process has been terminated.
- (void)webControllerWebProcessDidCrash:(CRWWebController*)webController;

// Called when a PassKit file is downloaded. |data| should be the data from a
// PassKit file, but this is not guaranteed, and the delegate is responsible for
// error handling non PassKit data using -[PKPass initWithData:error:]. If the
// download does not successfully complete, |data| will be nil.
- (void)webController:(CRWWebController*)webController
    didLoadPassKitObject:(NSData*)data;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
