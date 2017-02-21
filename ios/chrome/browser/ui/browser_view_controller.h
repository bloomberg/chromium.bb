// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_

#import <MessageUI/MessageUI.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_presenter.h"

@class BrowserContainerView;
@class BrowserViewControllerDependencyFactory;
@class ContextualSearchController;
@class ContextualSearchPanelView;
@class FindBarControllerIOS;
class GURL;
@class NoTabsController;
@class PageInfoViewController;
@class PreloadController;
@class PrintController;
@class SideSwipeController;
@class Tab;
@class TabModel;
@class TabStripController;
@class ThumbnailHelper;
@class VoiceSearchBarView;

namespace ios {
class ChromeBrowserState;
}

namespace ios_internal {
// Notification sent when the page info is shown.
extern NSString* const kPageInfoWillShowNotification;
// Notification sent when the page info is hidden.
extern NSString* const kPageInfoWillHideNotification;
// Notification sent when the location bar becomes first responder.
extern NSString* const kLocationBarBecomesFirstResponderNotification;
// Notification sent when the location bar resigns first responder.
extern NSString* const kLocationBarResignsFirstResponderNotification;
}  // namespace ios_internal

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController : UIViewController<SideSwipeControllerDelegate,
                                                    ToolbarOwner,
                                                    UrlLoader,
                                                    VoiceSearchPresenter,
                                                    WebToolbarDelegate>

// Initializes a new BVC from its nib. |model| must not be nil. The
// webUsageSuspended property for this BVC will be based on |model|, and future
// changes to |model|'s suspension state should be made through this BVC
// instead of directly on the model.
- (instancetype)initWithTabModel:(TabModel*)model
                    browserState:(ios::ChromeBrowserState*)browserState
               dependencyFactory:
                   (BrowserViewControllerDependencyFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The top-level browser container view.
@property(nonatomic, retain) BrowserContainerView* contentArea;

// Invisible button used to dismiss the keyboard.
@property(nonatomic, retain) UIButton* typingShield;

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Returns whether or not text to speech is playing.
@property(nonatomic, assign, readonly, getter=isPlayingTTS) BOOL playingTTS;

// Returns the TabModel passed to the initializer.
@property(nonatomic, assign, readonly) TabModel* tabModel;

// Returns the ios::ChromeBrowserState passed to the initializer.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;

// Whether the receiver is currently the primary BVC.
- (void)setPrimary:(BOOL)primary;

// Called when the typing shield is tapped.
- (void)shieldWasTapped:(id)sender;

// Called when a UI element to create a new tab is triggered.
- (void)newTab:(id)sender;

// Makes sure that the view hierarchy has been built. Equivalent to calling
// -view, but without the annoying compiler warning.
- (void)ensureViewCreated;

// Called when the browser state provided to this instance is being destroyed.
// At this point the browser will no longer ever be active, and will likely be
// deallocated soon.
- (void)browserStateDestroyed;

// Add a new tab with the given url, appends it to the end of the model,
// and makes it the selected tab. The selected tab is returned.
- (Tab*)addSelectedTabWithURL:(const GURL&)url
                   transition:(ui::PageTransition)transition;

// Add a new tab with the given url, at the given |position|,
// and makes it the selected tab. The selected tab is returned.
// If |position| == NSNotFound the tab will be added at the end of the stack.
- (Tab*)addSelectedTabWithURL:(const GURL&)url
                      atIndex:(NSUInteger)position
                   transition:(ui::PageTransition)transition;

// This will dismiss the web views on all the tabs and reload the frontmost one
// if there is one. This is used when a userdefault changes and the web views
// need to be re-created to pick it up.
- (void)resetAllWebViews;

// Informs the BVC that a new foreground tab is about to be opened. This is
// intended to be called before setWebUsageSuspended:NO in cases where a new tab
// is about to appear in order to allow the BVC to avoid doing unnecessary work
// related to showing the previously selected tab.
- (void)expectNewForegroundTab;

// Shows the voice search UI.
- (void)startVoiceSearch;

// Shows the QR scanner UI.
- (void)showQRScanner;

// Focuses the omnibox.
- (void)focusOmnibox;

// Dismisses all presented views then calls |completion|.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion;

// Returns a set with the names of the files received from other applications
// that are bookmarked or referenced by an open or recently closed tab.
- (NSSet*)referencedExternalFiles;

// Removes files received from other applications. If |immediately| is YES,
// initiates the removal of files immediately. |completionHandler| is called
// when files have been removed.
- (void)removeExternalFilesImmediately:(BOOL)immediately
                     completionHandler:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_
