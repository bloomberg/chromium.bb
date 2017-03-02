// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_H_
#define IOS_CHROME_BROWSER_TABS_TAB_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <vector>

#import "components/signin/ios/browser/manage_accounts_delegate.h"
#include "ios/net/request_tracker.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#include "ui/base/page_transition_types.h"

@class AutofillController;
@class AutoReloadBridge;
@class CastController;
@protocol CRWNativeContentProvider;
@class CRWWebController;
@class ExternalAppLauncher;
@class FormInputAccessoryViewController;
@class FullScreenController;
@protocol FullScreenControllerDelegate;
class GURL;
@class NativeAppNavigationController;
@class OpenInController;
@class OverscrollActionsController;
@protocol OverscrollActionsControllerDelegate;
@protocol PassKitDialogProvider;
@class PasswordController;
@class SnapshotManager;
@protocol SnapshotOverlayProvider;
@protocol StoreKitLauncher;
@class FormSuggestionController;
@protocol TabDelegate;
@protocol TabDialogDelegate;
@class Tab;
@protocol TabHeadersDelegate;
@class TabModel;
@protocol TabSnapshottingDelegate;

namespace infobars {
class InfoBarManager;
}

namespace ios {
class ChromeBrowserState;
}

namespace sessions {
class SerializedNavigationEntry;
struct SessionTab;
}

namespace web {
class NavigationItem;
class NavigationManager;
class NavigationManagerImpl;
struct Referrer;
class WebState;
}

// Notification sent by a Tab when it starts to load a new URL. This
// notification must only be used for crash reporting as it is also sent for
// pre-rendered tabs.
extern NSString* const kTabUrlStartedLoadingNotificationForCrashReporting;

// Notification sent by a Tab when it is likely about to start loading a new
// URL. This notification must only be used for crash reporting as it is also
// sent for pre-rendered tabs.
extern NSString* const kTabUrlMayStartLoadingNotificationForCrashReporting;

// Notification sent by a Tab when it is showing an exportable file (e.g a pdf
// file.
extern NSString* const kTabIsShowingExportableNotificationForCrashReporting;

// Notification sent by a Tab when it is closing its current document, to go to
// another location.
extern NSString* const kTabClosingCurrentDocumentNotificationForCrashReporting;

// The key containing the URL in the userInfo for the
// kTabUrlStartedLoadingForCrashReporting and
// kTabUrlMayStartLoadingNotificationForCrashReporting notifications.
extern NSString* const kTabUrlKey;

// The header name and value for the data reduction proxy to request an image to
// be reloaded without optimizations.
extern NSString* const kProxyPassthroughHeaderName;
extern NSString* const kProxyPassthroughHeaderValue;

// Information related to a single tab. The CRWWebController is similar to
// desktop Chrome's TabContents in that it encapsulates rendering. Acts as the
// delegate for the CRWWebController in order to process info about pages having
// loaded.
@interface Tab : NSObject<CRWWebDelegate, ManageAccountsDelegate>

// Browser state associated with this Tab.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// TODO(crbug.com/546208): Eliminate this; replace calls with either visible URL
// or last committed URL, depending on the specific use case.
// Do not add new calls to this method.
@property(nonatomic, readonly) const GURL& url;

// The Passkit Dialog provider used to show the UI to download a passkit object.
@property(nonatomic, assign) id<PassKitDialogProvider> passKitDialogProvider;

// The current title of the tab.
@property(nonatomic, readonly) NSString* title;

// Original page title or nil if the page did not provide one.
@property(nonatomic, readonly) NSString* originalTitle;

@property(nonatomic, readonly) NSString* urlDisplayString;

// ID associated with this tab.
@property(nonatomic, readonly) NSString* tabId;

// ID of the opener of this tab.
@property(nonatomic, readonly) NSString* openerID;

// NavigationIndex of the opener of this tab.
@property(nonatomic, readonly) NSInteger openerNavigationIndex;

// |YES| if snapshot overlay should load from the grey image cache.
@property(nonatomic, assign) BOOL useGreyImageCache;

// The Webstate associated with this Tab.
@property(nonatomic, readonly) web::WebState* webState;

@property(nonatomic, readonly) CRWWebController* webController;
@property(nonatomic, readonly) PasswordController* passwordController;
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;
@property(nonatomic, assign) id<TabDelegate> delegate;
@property(nonatomic, assign) id<TabHeadersDelegate> tabHeadersDelegate;
@property(nonatomic, assign) id<TabSnapshottingDelegate>
    tabSnapshottingDelegate;

// Whether or not desktop user agent is used for the currently visible page.
@property(nonatomic, readonly) BOOL usesDesktopUserAgent;

@property(nonatomic, assign) id<StoreKitLauncher> storeKitLauncher;
@property(nonatomic, assign) id<FullScreenControllerDelegate>
    fullScreenControllerDelegate;
@property(nonatomic, readonly)
    OverscrollActionsController* overscrollActionsController;
@property(nonatomic, assign) id<OverscrollActionsControllerDelegate>
    overscrollActionsControllerDelegate;
@property(nonatomic, assign) id<SnapshotOverlayProvider>
    snapshotOverlayProvider;

// Delegate used to show HTTP Authentication dialogs.
@property(nonatomic, weak) id<TabDialogDelegate> dialogDelegate;

// TODO(crbug.com/661663): Should this property abstract away the concept of
// prerendering?  Maybe this can move to the TabDelegate interface.
@property(nonatomic, assign) BOOL isPrerenderTab;
@property(nonatomic, assign) BOOL isLinkLoadingPrerenderTab;
@property(nonatomic, assign) BOOL isVoiceSearchResultsTab;

// |YES| if the tab has finished loading.
@property(nonatomic, readonly) BOOL loadFinished;

// Creates a new tab with the given state. |opener| is nil unless another tab
// is conceptually the parent of this tab. |openedByDOM| is YES if the page was
// opened by DOM. |model| and |browserState| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              opener:(Tab*)opener
                         openedByDOM:(BOOL)openedByDOM
                               model:(TabModel*)parentModel;

// Create a new tab with given web state and tab model. All must be non-nil.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                           model:(TabModel*)parentModel;

// Create a new tab with given web state and tab model, optionally attaching
// the tab helpers (controlled by |attachTabHelpers|). All must be non-nil.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                           model:(TabModel*)parentModel
                attachTabHelpers:(BOOL)attachTabHelpers
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Creates a new Tab instance loading |url| with |transition|, configured
// with no TabModel. |opener| may be nil, and behaves exactly as for
// -initWithBrowserState:opener:openedByDOM:model. |configuration| is a block
// that will be run before |url| starts loading, and is the correct place to set
// properties and delegates on the tab. Calling code must take ownership of the
// tab -- this is particularly important with Tab instances, because they will
// fail a DCHECK if they are deallocated without -close being called.
+ (Tab*)newPreloadingTabWithBrowserState:(ios::ChromeBrowserState*)browserState
                                     url:(const GURL&)url
                                referrer:(const web::Referrer&)referrer
                              transition:(ui::PageTransition)transition
                                provider:(id<CRWNativeContentProvider>)provider
                                  opener:(Tab*)opener
                        desktopUserAgent:(BOOL)desktopUserAgent
                           configuration:(void (^)(Tab*))configuration;

// Sets the parent tab model for this tab.  Can only be called if the tab does
// not already have a parent tab model set.
// TODO(crbug.com/228575): Create a delegate interface and remove this.
- (void)setParentTabModel:(TabModel*)model;

// Replace the content of the tab with the content described by |SessionTab|.
- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab;

// Stop the page loading.
// Equivalent to the user pressing 'stop', or a window.stop() command.
- (void)stopLoading;

// Triggers the asynchronous loading of the tab's favicon. This will be done
// automatically when a page loads, but this can be used to trigger favicon
// fetch earlier (e.g., for a tab that will be shown without loading).
- (void)fetchFavicon;

// Returns the favicon for the page currently being shown in this Tab, or |nil|
// if the current page has no favicon.
- (UIImage*)favicon;

// The view to display in the view hierarchy based on the current URL. Won't be
// nil. It is up to the caller to size the view and confirm |webUsageEnabled|.
- (UIView*)view;

// The view that generates print data when printing. It can be nil when printing
// is not supported with this tab. It can be different from |Tab view|.
- (UIView*)viewForPrinting;

// Halts the tab's network activity without closing it. This should only be
// called during shutdown, since the tab will be unusable but still present
// after this method completes.
- (void)terminateNetworkActivity;

// Starts the tab's shutdown sequence.
- (void)close;

// Dismisses all modals owned by the tab.
- (void)dismissModals;

// Opens StoreKit modal to download a native application identified with
// |appId|.
- (void)openAppStore:(NSString*)appId;

// Returns the NavigationManager for this tab's WebState. Requires WebState to
// be populated. Can return null.
// TODO(crbug.com/620465): remove navigationManagerImpl once Tab no longer uses
// nor exposes private ios/web/ API.
- (web::NavigationManager*)navigationManager;
- (web::NavigationManagerImpl*)navigationManagerImpl;

// Update the tab's history by replacing all previous navigations with
// |navigations|.
- (void)replaceHistoryWithNavigations:
            (const std::vector<sessions::SerializedNavigationEntry>&)navigations
                         currentIndex:(NSInteger)currentIndex;

// Navigate forwards or backwards to |item|.
- (void)goToItem:(const web::NavigationItem*)item;
- (void)reload;

// Navigates forwards or backwards.
// TODO(crbug.com/661664): These are passthroughs to CRWWebController. Convert
// all callers and remove these methods.
- (void)goBack;
- (void)goForward;

// Records the state (scroll position, form values, whatever can be
// harvested) from the current page into the current session entry.
- (void)recordStateInHistory;

// Returns the timestamp of the last time the tab is visited.
- (double)lastVisitedTimestamp;

// Updates the timestamp of the last time the tab is visited.
- (void)updateLastVisitedTimestamp;

// Returns the infobars::InfoBarManager object for this tab.
- (infobars::InfoBarManager*)infoBarManager;

// Whether the content of the current tab is compatible with reader mode.
- (BOOL)canSwitchToReaderMode;

// Asks the tab to enter into reader mode, presenting a streamlined view of the
// current content.
- (void)switchToReaderMode;

// Update internal state to use the desktop user agent. Must call
// -reloadWebViewAndURL for changes to take effect.
- (void)enableDesktopUserAgent;

// Remove the UIWebView and reload the current url.  Used by request desktop
// so the updated user agent is used.
- (void)reloadForDesktopUserAgent;

// Ensures the toolbar visibility matches |visible|.
- (void)updateFullscreenWithToolbarVisible:(BOOL)visible;

// Returns a snapshot of the current page, backed by disk so it can be purged
// and reloaded easily. The snapshot may be in memory, saved on disk or not
// present at all.
// 1) If the snapshot is in memory |block| will be called synchronously with
// the existing image.
// 2) If the snapshot is saven on disk |block| will be called asynchronously
// once the image is retrieved.
// 3) If the snapshot is not present at all |block| will be called
// asynchronously with a new snapshot.
// It is possible for |block| to not be called at all if there is no way to
// build a snapshot. |block| will always call back on the original thread.
- (void)retrieveSnapshot:(void (^)(UIImage*))callback;

// Invalidates the cached snapshot for the webcontroller's current session and
// forces a more recent snapshot to be generated and stored. Returns the
// snapshot with or without the overlayed views (e.g. infobar, voice search
// button, etc.), and either of the visible frame or of the full screen.
- (UIImage*)updateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                     visibleFrameOnly:(BOOL)visibleFrameOnly;

// Snaps a snapshot image for the current page including optional infobars.
// Returns an autoreleased image cropped and scaled appropriately, with or
// without the overlayed views (e.g. infobar, voice search button, etc.), and
// either of the visible frame or of the full screen.
// Returns nil if a snapshot cannot be generated.
- (UIImage*)generateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                       visibleFrameOnly:(BOOL)visibleFrameOnly;

// When snapshot coalescing is enabled, mutiple calls to generate a snapshot
// with the same parameters may be coalesced.
- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled;

// Returns the rect (in |view|'s coordinate space) to include for snapshotting.
- (CGRect)snapshotContentArea;

// Called when the snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Enables or disables usage of web views inside the Tab.
- (void)setWebUsageEnabled:(BOOL)webUsageEnabled;

// Returns the NativeAppNavigationController for this tab.
- (NativeAppNavigationController*)nativeAppNavigationController;

// Called when this tab is shown.
- (void)wasShown;

// Called when this tab is hidden.
- (void)wasHidden;

// Evaluates U2F result.
- (void)evaluateU2FResultFromURL:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_H_
