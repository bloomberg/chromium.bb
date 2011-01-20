// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#pragma once

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/sync_ui_util.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"


class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
class ConstrainedWindowMac;
@class DevToolsController;
@class DownloadShelfController;
@class FindBarCocoaController;
@class FullscreenController;
@class GTMWindowSheetController;
@class IncognitoImageView;
@class InfoBarContainerController;
class LocationBarViewMac;
@class PreviewableContentsController;
@class SidebarController;
class StatusBubbleMac;
class TabContents;
@class TabStripController;
@class TabStripView;
@class ToolbarController;


@interface BrowserWindowController :
  TabWindowController<NSUserInterfaceValidations,
                      BookmarkBarControllerDelegate,
                      BrowserCommandExecutor,
                      ViewResizer,
                      TabContentsControllerDelegate,
                      TabStripControllerDelegate> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc).
  scoped_ptr<Browser> browser_;
  NSWindow* savedRegularWindow_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
  scoped_nsobject<ToolbarController> toolbarController_;
  scoped_nsobject<TabStripController> tabStripController_;
  scoped_nsobject<FindBarCocoaController> findBarCocoaController_;
  scoped_nsobject<InfoBarContainerController> infoBarContainerController_;
  scoped_nsobject<DownloadShelfController> downloadShelfController_;
  scoped_nsobject<BookmarkBarController> bookmarkBarController_;
  scoped_nsobject<DevToolsController> devToolsController_;
  scoped_nsobject<SidebarController> sidebarController_;
  scoped_nsobject<PreviewableContentsController> previewableContentsController_;
  scoped_nsobject<FullscreenController> fullscreenController_;

  // Strong. StatusBubble is a special case of a strong reference that
  // we don't wrap in a scoped_ptr because it is acting the same
  // as an NSWindowController in that it wraps a window that must
  // be shut down before our destructors are called.
  StatusBubbleMac* statusBubble_;

  BookmarkBubbleController* bookmarkBubbleController_;  // Weak.
  BOOL initializing_;  // YES while we are currently in initWithBrowser:
  BOOL ownsBrowser_;  // Only ever NO when testing

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

  // The view which shows the incognito badge (NULL if not an incognito window).
  // Needed to access the view to move it to/from the fullscreen window.
  scoped_nsobject<IncognitoImageView> incognitoBadge_;

  // Lazily created view which draws the background for the floating set of bars
  // in fullscreen mode (for window types having a floating bar; it remains nil
  // for those which don't).
  scoped_nsobject<NSView> floatingBarBackingView_;

  // Tracks whether the floating bar is above or below the bookmark bar, in
  // terms of z-order.
  BOOL floatingBarAboveBookmarkBar_;

  // The proportion of the floating bar which is shown (in fullscreen mode).
  CGFloat floatingBarShownFraction_;

  // Various UI elements/events may want to ensure that the floating bar is
  // visible (in fullscreen mode), e.g., because of where the mouse is or where
  // keyboard focus is. Whenever an object requires bar visibility, it has
  // itself added to |barVisibilityLocks_|. When it no longer requires bar
  // visibility, it has itself removed.
  scoped_nsobject<NSMutableSet> barVisibilityLocks_;

  // Bar visibility locks and releases only result (when appropriate) in changes
  // in visible state when the following is |YES|.
  BOOL barVisibilityUpdatesEnabled_;
}

// A convenience class method which gets the |BrowserWindowController| for a
// given window. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForWindow:(NSWindow*)window;

// A convenience class method which gets the |BrowserWindowController| for a
// given view.  This is the controller for the window containing |view|, if it
// is a BWC, or the first controller in the parent-window chain that is a
// BWC. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForView:(NSView*)view;

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// Call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Return a weak pointer to the toolbar controller.
- (ToolbarController*)toolbarController;

// Return a weak pointer to the tab strip controller.
- (TabStripController*)tabStripController;

// Access the C++ bridge object representing the status bubble for the window.
- (StatusBubbleMac*)statusBubble;

// Access the C++ bridge object representing the location bar.
- (LocationBarViewMac*)locationBarBridge;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Called to tell the selected tab to update its loading state.
// |force| is set if the update is due to changing tabs, as opposed to
// the page-load finishing.  See comment in reload_button.h.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

// Brings this controller's window to the front.
- (void)activate;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar:(BOOL)selectAll;

// Make the (currently-selected) tab contents the first responder, if possible.
- (void)focusTabContents;

// Returns the frame of the regular (non-fullscreened) window (even if the
// window is currently in fullscreen mode).  The frame is returned in Cocoa
// coordinates (origin in bottom-left).
- (NSRect)regularWindowFrame;

- (BOOL)isBookmarkBarVisible;

// Returns YES if the bookmark bar is currently animating.
- (BOOL)isBookmarkBarAnimating;

// Called after bookmark bar visibility changes (due to pref change or change in
// tab/tab contents).
- (void)updateBookmarkBarVisibilityWithAnimation:(BOOL)animate;

- (BOOL)isDownloadShelfVisible;

// Lazily creates the download shelf in visible state if it doesn't exist yet.
- (DownloadShelfController*)downloadShelf;

// Retains the given FindBarCocoaController and adds its view to this
// browser window.  Must only be called once per
// BrowserWindowController.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// The user changed the theme.
- (void)userChangedTheme;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_command_ids.h" file.
- (void)executeCommand:(int)command;

// Delegate method for the status bubble to query its base frame.
- (NSRect)statusBubbleBaseFrame;

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
// Returns NO if constrained windows cannot be attached to this window.
- (BOOL)canAttachConstrainedWindow;

// Shows or hides the docked web inspector depending on |contents|'s state.
- (void)updateDevToolsForContents:(TabContents*)contents;

// Displays the active sidebar linked to the |contents| or hides sidebar UI,
// if there's no such sidebar.
- (void)updateSidebarForContents:(TabContents*)contents;

// Gets the current theme provider.
- (ui::ThemeProvider*)themeProvider;

// Gets the window style.
- (ThemedWindowStyle)themedWindowStyle;

// Gets the pattern phase for the window.
- (NSPoint)themePatternPhase;

// Return the point to which a bubble window's arrow should point.
- (NSPoint)bookmarkBubblePoint;

// Call when the user changes the tab strip display mode, enabling or
// disabling vertical tabs for this browser. Re-flows the contents of the
// browser.
- (void)toggleTabStripDisplayMode;

// Shows or hides the Instant preview contents.
- (void)showInstant:(TabContents*)previewContents;
- (void)hideInstant;

// Returns the frame, in Cocoa (unflipped) screen coordinates, of the area where
// Instant results are.  If Instant is not showing, returns the frame of where
// it would be.
- (NSRect)instantFrame;

// Called when the Add Search Engine dialog is closed.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context;

@end  // @interface BrowserWindowController


// Methods having to do with the window type (normal/popup/app, and whether the
// window has various features; fullscreen methods are separate).
@interface BrowserWindowController(WindowType)

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
// Note: The |-has...| methods are usually preferred, so this method is largely
// deprecated.
- (BOOL)isNormalWindow;

@end  // @interface BrowserWindowController(WindowType)


// Methods having to do with fullscreen mode.
@interface BrowserWindowController(Fullscreen)

// Enters (or exits) fullscreen mode.
- (void)setFullscreen:(BOOL)fullscreen;

// Returns fullscreen state.
- (BOOL)isFullscreen;

// Resizes the fullscreen window to fit the screen it's currently on.  Called by
// the FullscreenController when there is a change in monitor placement or
// resolution.
- (void)resizeFullscreenWindow;

// Gets or sets the fraction of the floating bar (fullscreen overlay) that is
// shown.  0 is completely hidden, 1 is fully shown.
- (CGFloat)floatingBarShownFraction;
- (void)setFloatingBarShownFraction:(CGFloat)fraction;

// Query/lock/release the requirement that the tab strip/toolbar/attached
// bookmark bar bar cluster is visible (e.g., when one of its elements has
// focus). This is required for the floating bar in fullscreen mode, but should
// also be called when not in fullscreen mode; see the comments for
// |barVisibilityLocks_| for more details. Double locks/releases by the same
// owner are ignored. If |animate:| is YES, then an animation may be performed,
// possibly after a small delay if |delay:| is YES. If |animate:| is NO,
// |delay:| will be ignored. In the case of multiple calls, later calls have
// precedence with the rule that |animate:NO| has precedence over |animate:YES|,
// and |delay:NO| has precedence over |delay:YES|.
- (BOOL)isBarVisibilityLockedForOwner:(id)owner;
- (void)lockBarVisibilityForOwner:(id)owner
                    withAnimation:(BOOL)animate
                            delay:(BOOL)delay;
- (void)releaseBarVisibilityForOwner:(id)owner
                       withAnimation:(BOOL)animate
                               delay:(BOOL)delay;

// Returns YES if any of the views in the floating bar currently has focus.
- (BOOL)floatingBarHasFocus;

// Opens the tabpose window.
- (void)openTabpose;

@end  // @interface BrowserWindowController(Fullscreen)


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
- (NSWindow*)createFullscreenWindow;

// Resets any saved state about window growth (due to showing the bookmark bar
// or the download shelf), so that future shrinking will occur from the bottom.
- (void)resetWindowGrowthState;

// Computes by how far in each direction, horizontal and vertical, the
// |source| rect doesn't fit into |target|.
- (NSSize)overflowFrom:(NSRect)source
                    to:(NSRect)target;
@end  // @interface BrowserWindowController(TestingAPI)


#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
