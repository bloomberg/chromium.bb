// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_H_
#define IOS_CHROME_BROWSER_TABS_TAB_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <vector>

#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#include "ios/net/request_tracker.h"
#include "ios/web/public/user_agent.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#include "ui/base/page_transition_types.h"

@class AutofillController;
@class AutoReloadBridge;
@class CastController;
@class CRWWebController;
@class ExternalAppLauncher;
@class FormInputAccessoryViewController;
@class FullScreenController;
@protocol FullScreenControllerDelegate;
class GURL;
@class OpenInController;
@class OverscrollActionsController;
@protocol OverscrollActionsControllerDelegate;
@protocol PassKitDialogProvider;
@class PasswordController;
@class SnapshotManager;
@protocol SnapshotOverlayProvider;
@class FormSuggestionController;
@protocol TabDelegate;
@protocol TabDialogDelegate;
@class Tab;
@protocol TabHeadersDelegate;
@class TabModel;
@protocol TabSnapshottingDelegate;
@protocol FindInPageControllerDelegate;

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
@interface Tab
    : NSObject<CRWWebDelegate, ManageAccountsDelegate, SadTabTabHelperDelegate>

// Browser state associated with this Tab.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Returns the URL of the last committed NavigationItem for this Tab.
@property(nonatomic, readonly) const GURL& lastCommittedURL;

// Returns the URL of the visible NavigationItem for this Tab.
@property(nonatomic, readonly) const GURL& visibleURL;

// The Passkit Dialog provider used to show the UI to download a passkit object.
@property(nonatomic, weak) id<PassKitDialogProvider> passKitDialogProvider;

// The current title of the tab.
@property(nonatomic, readonly) NSString* title;

// Original page title or nil if the page did not provide one.
@property(nonatomic, readonly) NSString* originalTitle;

@property(nonatomic, readonly) NSString* urlDisplayString;

// ID associated with this tab.
@property(nonatomic, readonly) NSString* tabId;

// |YES| if snapshot overlay should load from the grey image cache.
@property(nonatomic, assign) BOOL useGreyImageCache;

// The Webstate associated with this Tab.
@property(nonatomic, readonly) web::WebState* webState;

@property(nonatomic, readonly) CRWWebController* webController;

// Handles saving and autofill of passwords.
@property(nonatomic, readonly) PasswordController* passwordController;
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;
@property(nonatomic, weak) id<TabDelegate> delegate;
@property(nonatomic, weak) id<TabHeadersDelegate> tabHeadersDelegate;
@property(nonatomic, weak) id<TabSnapshottingDelegate> tabSnapshottingDelegate;
@property(nonatomic, readonly) id<FindInPageControllerDelegate>
    findInPageControllerDelegate;

// Whether or not desktop user agent is used for the currently visible page.
@property(nonatomic, readonly) BOOL usesDesktopUserAgent;

@property(nonatomic, weak) id<FullScreenControllerDelegate>
    fullScreenControllerDelegate;
@property(nonatomic, readonly)
    OverscrollActionsController* overscrollActionsController;
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollActionsControllerDelegate;
@property(nonatomic, weak) id<SnapshotOverlayProvider> snapshotOverlayProvider;

// Delegate used to show HTTP Authentication dialogs.
@property(nonatomic, weak) id<TabDialogDelegate> dialogDelegate;

// TODO(crbug.com/661663): Should this property abstract away the concept of
// prerendering?  Maybe this can move to the TabDelegate interface.
@property(nonatomic, assign) BOOL isPrerenderTab;
@property(nonatomic, assign) BOOL isLinkLoadingPrerenderTab;
@property(nonatomic, assign) BOOL isVoiceSearchResultsTab;

// |YES| if the tab has finished loading.
@property(nonatomic, readonly) BOOL loadFinished;

// Creates a new Tab with the given WebState.
- (instancetype)initWithWebState:(web::WebState*)webState;

- (instancetype)init NS_UNAVAILABLE;

// Sets the parent tab model for this tab.  Can only be called if the tab does
// not already have a parent tab model set.
// TODO(crbug.com/228575): Create a delegate interface and remove this.
- (void)setParentTabModel:(TabModel*)model;

// Replace the content of the tab with the content described by |SessionTab|.
- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab;

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

// Dismisses all modals owned by the tab.
- (void)dismissModals;

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

// Navigates forwards or backwards.
// TODO(crbug.com/661664): These are passthroughs to CRWWebController. Convert
// all callers and remove these methods.
- (void)goBack;
- (void)goForward;

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

// Loads the original url of the last non-redirect item (including non-history
// items). Used by request desktop/mobile site so that the updated user agent is
// used.
- (void)reloadWithUserAgentType:(web::UserAgentType)userAgentType;

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

// Called when this tab is shown.
- (void)wasShown;

// Called when this tab is hidden.
- (void)wasHidden;

// Evaluates U2F result.
- (void)evaluateU2FResultFromURL:(const GURL&)url;

// Cancels prerendering. It is an error to call this on anything except a
// prerender tab (where |isPrerenderTab| is set to YES).
- (void)discardPrerender;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_H_
