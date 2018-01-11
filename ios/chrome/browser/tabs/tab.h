// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_H_
#define IOS_CHROME_BROWSER_TABS_TAB_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <vector>

#include "ios/net/request_tracker.h"
#include "ios/web/public/user_agent.h"
#include "ui/base/page_transition_types.h"

@class AutofillController;
@class CastController;
@class ExternalAppLauncher;
@class FormInputAccessoryViewController;
@class LegacyFullscreenController;
@protocol LegacyFullscreenControllerDelegate;
class GURL;
@class OpenInController;
@class OverscrollActionsController;
@protocol OverscrollActionsControllerDelegate;
@protocol PassKitDialogProvider;
@class PasswordController;
@class SnapshotManager;
@class FormSuggestionController;
@protocol TabDelegate;
@protocol TabDialogDelegate;
@class Tab;
@protocol TabHeadersDelegate;
@class TabModel;
@protocol FindInPageControllerDelegate;

namespace ios {
class ChromeBrowserState;
}

namespace web {
class NavigationItem;
class NavigationManager;
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

// Information related to a single tab. The WebState is similar to desktop
// Chrome's WebContents in that it encapsulates rendering. Acts as the
// delegate for the WebState in order to process info about pages having
// loaded.
@interface Tab : NSObject

// Browser state associated with this Tab.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// The Passkit Dialog provider used to show the UI to download a passkit object.
@property(nonatomic, weak) id<PassKitDialogProvider> passKitDialogProvider;

// The current title of the tab.
@property(nonatomic, readonly) NSString* title;

@property(nonatomic, readonly) NSString* urlDisplayString;

// ID associated with this tab.
@property(nonatomic, readonly) NSString* tabId;

// The Webstate associated with this Tab.
@property(nonatomic, readonly) web::WebState* webState;

@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;
@property(nonatomic, weak) id<TabDelegate> delegate;
@property(nonatomic, weak) id<TabHeadersDelegate> tabHeadersDelegate;
@property(nonatomic, readonly) id<FindInPageControllerDelegate>
    findInPageControllerDelegate;

// Whether or not desktop user agent is used for the currently visible page.
@property(nonatomic, readonly) BOOL usesDesktopUserAgent;

// The delegate to use for the legacy fullscreen controller.  It should not be
// set if the new fullscreen is enabled.
// TODO(crbug.com/778823): Remove this property.
@property(nonatomic, weak) id<LegacyFullscreenControllerDelegate>
    legacyFullscreenControllerDelegate;

@property(nonatomic, readonly)
    OverscrollActionsController* overscrollActionsController;
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollActionsControllerDelegate;

// Delegate used to show HTTP Authentication dialogs.
@property(nonatomic, weak) id<TabDialogDelegate> dialogDelegate;

// Whether this tab is displaying a voice search result.
@property(nonatomic, readonly) BOOL isVoiceSearchResultsTab;

// |YES| if the tab has finished loading.
@property(nonatomic, readonly) BOOL loadFinished;

// Creates a new Tab with the given WebState.
- (instancetype)initWithWebState:(web::WebState*)webState;

- (instancetype)init NS_UNAVAILABLE;

// Sets the parent tab model for this tab.  Can only be called if the tab does
// not already have a parent tab model set.
// TODO(crbug.com/228575): Create a delegate interface and remove this.
- (void)setParentTabModel:(TabModel*)model;

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
- (web::NavigationManager*)navigationManager;

// Navigate forwards or backwards to |item|.
- (void)goToItem:(const web::NavigationItem*)item;

// Navigates forwards or backwards.
// TODO(crbug.com/661664): These are passthroughs to the Tab's WebState's
// NavigationManager. Convert all callers and remove these methods.
- (void)goBack;
- (void)goForward;

// Returns the timestamp of the last time the tab is visited.
- (double)lastVisitedTimestamp;

// Updates the timestamp of the last time the tab is visited.
- (void)updateLastVisitedTimestamp;

// Loads the original url of the last non-redirect item (including non-history
// items). Used by request desktop/mobile site so that the updated user agent is
// used.
- (void)reloadWithUserAgentType:(web::UserAgentType)userAgentType;

// Ensures the toolbar visibility matches |visible|.
// TODO(crbug.com/778823): Remove this code.
- (void)updateFullscreenWithToolbarVisible:(BOOL)visible;

// Called when this tab is shown.
- (void)wasShown;

// Called when this tab is hidden.
- (void)wasHidden;

// Called before capturing a snapshot for Tab.
- (void)willUpdateSnapshot;

// Evaluates U2F result.
- (void)evaluateU2FResultFromURL:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_H_
