// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.
// Note that under the hood the BrowserWindowController is neither an
// NSWindowController nor its window's delegate, though it receives all
// NSWindowDelegate methods as if it were.

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/gfx/geometry/rect.h"

class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
@class BrowserWindowTouchBarController;
class ExtensionKeybindingRegistryCocoa;
class LocationBarViewMac;
@class OverlayableContentsController;

namespace content {
class WebContents;
}

namespace extensions {
class Command;
}

constexpr const gfx::Size kMinCocoaTabbedWindowSize(400, 272);
constexpr const gfx::Size kMinCocoaPopupWindowSize(100, 122);

@interface BrowserWindowController : TabWindowController<ViewResizer> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc).
  std::unique_ptr<Browser> browser_;
  NSWindow* savedRegularWindow_;
  std::unique_ptr<BrowserWindowCocoa> windowShim_;
  std::unique_ptr<LocationBarViewMac> locationBar_;
  base::scoped_nsobject<OverlayableContentsController>
      overlayableContentsController_;
  base::scoped_nsobject<BrowserWindowTouchBarController> touchBarController_;

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

  // If this ivar is set to YES, layoutSubviews calls will be ignored. This is
  // used in fullscreen transition to prevent spurious resize messages from
  // being sent to the renderer, which causes the transition to be janky.
  BOOL blockLayoutSubviews_;

  // Set when AppKit invokes -windowWillClose: to protect against possible
  // crashes. See http://crbug.com/671213.
  BOOL didWindowWillClose_;

  // The Extension Command Registry used to determine which keyboard events to
  // handle.
  std::unique_ptr<ExtensionKeybindingRegistryCocoa>
      extensionKeybindingRegistry_;
}

// A convenience class method which returns the |BrowserWindowController| for
// |window|, or nil if neither |window| nor its parent or any other ancestor
// has one.
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

// Ensure bounds for the window abide by the minimum window size.
- (gfx::Rect)enforceMinWindowSize:(gfx::Rect)bounds;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Access the C++ bridge object representing the location bar.
- (LocationBarViewMac*)locationBarBridge;

// Returns a weak pointer to the overlayable contents controller.
- (OverlayableContentsController*)overlayableContentsController;

// Access the Profile object that backs this Browser.
- (Profile*)profile;

// Forces the toolbar (and transitively the location bar) to update its current
// state.  If |tab| is non-NULL, we're switching (back?) to this tab and should
// restore any previous location bar state (such as user editing) as well.
- (void)updateToolbarWithContents:(content::WebContents*)tab;

// Resets the toolbar's tab state for |tab|.
- (void)resetTabState:(content::WebContents*)tab;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Sets whether or not the current page is translated.
- (void)setCurrentPageIsTranslated:(BOOL)on;

// Invoked via BrowserWindowCocoa::OnActiveTabChanged, happens whenever a
// new tab becomes active.
- (void)onActiveTabChanged:(content::WebContents*)oldContents
                        to:(content::WebContents*)newContents;

// Happens when the zoom level is changed in the active tab, the active tab is
// changed, or a new browser window or tab is created. |canShowBubble| denotes
// whether it would be appropriate to show a zoom bubble or not.
- (void)zoomChangedForActiveTab:(BOOL)canShowBubble;

// Called to tell the selected tab to update its loading state.
// |force| is set if the update is due to changing tabs, as opposed to
// the page-load finishing.  See comment in reload_button_cocoa.h.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

// Brings this controller's window to the front.
- (void)activate;

// Called by FrameBrowserWindow when |makeFirstResponder:| is called.
// This method checks to see if a view in TopChrome has the first responder
// status. If it does, it will lock the fullscreen toolbar so that the toolbar
// will remain dropped down when the user is still interacting with it via
// keyboard access. Otherwise, it will release the toolbar.
- (void)firstResponderUpdated:(NSResponder*)responder;

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

// The user changed the theme.
- (void)userChangedTheme;

// Consults the Command Registry to see if this |event| needs to be handled as
// an extension command and returns YES if so (NO otherwise).
// Only extensions with the given |priority| are considered.
- (BOOL)handledByExtensionCommand:(NSEvent*)event
    priority:(ui::AcceleratorManager::HandlerPriority)priority;

// Dismiss the permission bubble
- (void)dismissPermissionBubble;

// Shows or hides the docked web inspector depending on |contents|'s state.
- (void)updateDevToolsForContents:(content::WebContents*)contents;

// Gets the current theme provider.
- (const ui::ThemeProvider*)themeProvider;

// Gets the window style.
- (ThemedWindowStyle)themedWindowStyle;

// Returns the position in window coordinates that the top left of a theme
// image with |alignment| should be painted at. If the window does not have a
// tab strip, the offset for THEME_IMAGE_ALIGN_WITH_FRAME is always returned.
// The result of this method can be used in conjunction with
// [NSGraphicsContext cr_setPatternPhase:] to set the offset of pattern colors.
- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment;

// Called when the Add Search Engine dialog is closed.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context;

// Executes the command registered by the extension that has the given id.
- (void)executeExtensionCommand:(const std::string&)extension_id
                        command:(const extensions::Command&)command;

// Sets the alert state of the tab e.g. audio playing, media recording, etc.
// See TabUtils::TabAlertState for a list of all possible alert states.
- (void)setAlertState:(TabAlertState)alertState;

// Returns current alert state, determined by the alert state of tabs, set by
// UpdateAlertState.
- (TabAlertState)alertState;

// Returns the BrowserWindowTouchBarController object associated with the
// window.
- (BrowserWindowTouchBarController*)browserWindowTouchBarController
    API_AVAILABLE(macos(10.12.2));

// Indicates whether the toolbar is visible to the user. Toolbar is usually
// triggered by moving mouse cursor to the top of the monitor.
- (BOOL)isToolbarShowing;

@end  // @interface BrowserWindowController


// Methods having to do with the window type (normal/popup/app, and whether the
// window has various features.
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

// Called to check whether or not this window can have bookmark bar (YES if it
// does, NO otherwise). (E.g., normal browser windows may, pop-ups may not.)
- (BOOL)supportsBookmarkBar;

// Called to check if this controller's window is a tabbed window (e.g., not a
// pop-up window). Returns YES if it is, NO otherwise.
// Note: The |-has...| methods are usually preferred, so this method is largely
// deprecated.
- (BOOL)isTabbedWindow;

// Returns the size of the original (non-fullscreen) window.
- (NSRect)savedRegularWindowFrame;

@end  // @interface BrowserWindowController(WindowType)


// Methods which are either only for testing, or only public for testing.
@interface BrowserWindowController (TestingAPI)

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
// Returns if the window height was changed.
- (BOOL)adjustWindowHeightBy:(CGFloat)deltaH;

// Resets any saved state about window growth (due to showing the bookmark bar
// or the download shelf), so that future shrinking will occur from the bottom.
- (void)resetWindowGrowthState;

// Computes by how far in each direction, horizontal and vertical, the
// |source| rect doesn't fit into |target|.
- (NSSize)overflowFrom:(NSRect)source
                    to:(NSRect)target;

// Gets the rect, in window base coordinates, that the omnibox popup should be
// positioned relative to.
- (NSRect)omniboxPopupAnchorRect;

// Returns the flag |blockLayoutSubviews_|.
- (BOOL)isLayoutSubviewsBlocked;

// Returns the active tab contents controller's |blockFullscreenResize_| flag.
- (BOOL)isActiveTabContentsControllerResizeBlocked;

// Sets |touchbarController_|.
- (void)setBrowserWindowTouchBarController:
    (BrowserWindowTouchBarController*)controller API_AVAILABLE(macos(10.12.2));

@end  // @interface BrowserWindowController (TestingAPI)

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
