// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/tab_window_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#import "chrome/browser/cocoa/browser_command_executor.h"
#import "chrome/browser/cocoa/url_drop_target.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
@class ChromeBrowserWindow;
class ConstrainedWindowMac;
@class DownloadShelfController;
@class FindBarCocoaController;
@class GTMWindowSheetController;
@class InfoBarContainerController;
class LocationBar;
class StatusBubbleMac;
class TabContents;
@class TabStripController;
class TabStripModelObserverBridge;
@class TabStripView;
@class ToolbarController;
@class TitlebarController;

@interface BrowserWindowController :
  TabWindowController<NSUserInterfaceValidations,
                      BookmarkBarControllerDelegate,
                      BrowserCommandExecutor,
                      ViewResizer,
                      GTMThemeDelegate> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc).
  scoped_ptr<Browser> browser_;
  NSWindow* savedRegularWindow_;
  scoped_ptr<TabStripModelObserverBridge> tabObserver_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
  scoped_nsobject<ToolbarController> toolbarController_;
  scoped_nsobject<TitlebarController> titlebarController_;
  scoped_nsobject<TabStripController> tabStripController_;
  scoped_nsobject<FindBarCocoaController> findBarCocoaController_;
  scoped_nsobject<InfoBarContainerController> infoBarContainerController_;
  scoped_nsobject<DownloadShelfController> downloadShelfController_;
  scoped_nsobject<BookmarkBarController> bookmarkBarController_;

  // Strong. StatusBubble is a special case of a strong reference that
  // we don't wrap in a scoped_ptr because it is acting the same
  // as an NSWindowController in that it wraps a window that must
  // be shut down before our destructors are called.
  StatusBubbleMac* statusBubble_;

  BookmarkBubbleController* bookmarkBubbleController_;  // Weak.
  scoped_nsobject<GTMTheme> theme_;
  BOOL initializing_;  // YES while we are currently in initWithBrowser:
  BOOL ownsBrowser_;  // Only ever NO when testing
  CGFloat verticalOffsetForStatusBubble_;

  // The total amount by which we've grown the window up or down (to display a
  // bookmark bar and/or download shelf), respectively; reset to 0 when moved
  // away from the bottom/top or resized (or zoomed).
  CGFloat windowTopGrowth_;
  CGFloat windowBottomGrowth_;

  // YES only if we're shrinking the window from an apparent zoomed state (which
  // we'll only do if we grew it to the zoomed state); needed since we'll then
  // restrict the amount of shrinking by the amounts specified above. Reset to
  // NO on growth.
  BOOL isShrinkingFromZoomed_;

  // The raw accumulated zoom value and the actual zoom increments made for an
  // an in-progress pinch gesture.
  CGFloat totalMagnifyGestureAmount_;
  NSInteger currentZoomStepDelta_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// Call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Access the C++ bridge object representing the location bar.
- (LocationBar*)locationBarBridge;

// Access the C++ bridge object representing the status bubble for the window.
- (StatusBubbleMac*)statusBubble;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Called to tell the selected tab to update its loading state.
- (void)setIsLoading:(BOOL)isLoading;

// Brings this controller's window to the front.
- (void)activate;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Determines whether this controller's window supports a given feature (i.e.,
// whether a given feature is or can be shown in the window).
// TODO(viettrungluu): |feature| is really should be |Browser::Feature|, but I
// don't want to include browser.h (and you can't forward declare enums).
- (BOOL)supportsWindowFeature:(int)feature;

// Called to check whether or not this window has a normal title bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do not, pop-ups do.)
- (BOOL)hasTitleBar;

// Called to check whether or not this window has a toolbar (YES if it does, NO
// otherwise). (E.g., normal browser windows do, pop-ups do not.)
- (BOOL)hasToolbar;

// Called to check whether or not this window has a location bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do, pop-ups may or may
// not.)
- (BOOL)hasLocationBar;

// Called to check whether or not this window can have bookmark bar (YES if it
// does, NO otherwise). (E.g., normal browser windows may, pop-ups may not.)
- (BOOL)supportsBookmarkBar;

// Called to check if this controller's window is a normal window (e.g., not a
// pop-up window). Returns YES if it is, NO otherwise.
- (BOOL)isNormalWindow;

- (BOOL)isBookmarkBarVisible;

// Called after bookmark bar visibility changes (due to pref change or change in
// tab/tab contents).
- (void)updateBookmarkBarVisibilityWithAnimation:(BOOL)animate;

// Return a weak pointer to the tab strip controller.
- (TabStripController*)tabStripController;

// Return a weak pointer to the toolbar controller.
- (ToolbarController*)toolbarController;

- (BOOL)isDownloadShelfVisible;

// Lazily creates the download shelf in visible state if it doesn't exist yet.
- (DownloadShelfController*)downloadShelf;

// Retains the given FindBarCocoaController and adds its view to this
// browser window.  Must only be called once per
// BrowserWindowController.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// Enters (or exits) fullscreen mode.
- (void)setFullscreen:(BOOL)fullscreen;

// Returns fullscreen state.
- (BOOL)isFullscreen;

// The user changed the theme.
- (void)userChangedTheme;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_dll_resource.h" file.
- (void)executeCommand:(int)command;

// Delegate method for the status bubble to query about its vertical offset.
- (float)verticalOffsetForStatusBubble;

// Show the bookmark bubble (e.g. user just clicked on the STAR)
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyBookmarked;

// Returns the (lazily created) window sheet controller of this window. Used
// for the per-tab sheets.
- (GTMWindowSheetController*)sheetController;

// Requests that |window| is opened as a per-tab sheet to the current tab.
- (void)attachConstrainedWindow:(ConstrainedWindowMac*)window;
// Closes the tab sheet |window| and potentially shows the next sheet in the
// tab's sheet queue.
- (void)removeConstrainedWindow:(ConstrainedWindowMac*)window;

// Shows or hides the docked web inspector depending on |contents|'s state.
- (void)updateDevToolsForContents:(TabContents*)contents;

@end

// Methods which are either only for testing, or only public for testing.
@interface BrowserWindowController(TestingAPI)

// Put the incognito badge on the browser and adjust the tab strip
// accordingly.
- (void)installIncognitoBadge;

// Allows us to initWithBrowser withOUT taking ownership of the browser.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt;

// Adjusts the window height by the given amount.  If the window spans from the
// top of the current workspace to the bottom of the current workspace, the
// height is not adjusted.  If growing the window by the requested amount would
// size the window to be taller than the current workspace, the window height is
// capped to be equal to the height of the current workspace.  If the window is
// partially offscreen, its height is not adjusted at all.  This function
// prefers to grow the window down, but will grow up if needed.  Calls to this
// function should be followed by a call to |layoutSubviews|.
- (void)adjustWindowHeightBy:(CGFloat)deltaH;

// Return an autoreleased NSWindow suitable for fullscreen use.
- (NSWindow*)fullscreenWindow;

// Return a point suitable for the topLeft for a bookmark bubble.
- (NSPoint)topLeftForBubble;

// Resets any saved state about window growth (due to showing the bookmark bar
// or the download shelf), so that future shrinking will occur from the bottom.
- (void)resetWindowGrowthState;

@end  // BrowserWindowController(TestingAPI)

#endif  // CHROME_BROWSER_COCOA_BROWSER_WINDOW_CONTROLLER_H_
