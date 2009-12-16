// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/command_observer_bridge.h"
#import "chrome/browser/cocoa/delayedmenu_button.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/common/pref_member.h"

class AppMenuModel;
@class AutocompleteTextField;
@class AutocompleteTextFieldEditor;
@class BackForwardMenuController;
class Browser;
@class BrowserActionsController;
class BubblePositioner;
class CommandUpdater;
@class DelayedMenuButton;
class LocationBar;
class LocationBarViewMac;
@class MenuButton;
@class MenuController;
class PageMenuModel;
namespace ToolbarControllerInternal {
class MenuDelegate;
class PrefObserverBridge;
}
class Profile;
class TabContents;
class ToolbarModel;

// A controller for the toolbar in the browser window. Manages
// updating the state for location bar and back/fwd/reload/go buttons.
// Manages the bookmark bar and its position in the window relative to
// the web content view.

@interface ToolbarController :
    NSViewController<CommandObserverProtocol> {
 @private
  ToolbarModel* toolbarModel_;  // weak, one per window
  CommandUpdater* commands_;  // weak, one per window
  Profile* profile_;  // weak, one per window
  Browser* browser_;  // weak, one per window
  scoped_ptr<CommandObserverBridge> commandObserver_;
  scoped_ptr<LocationBarViewMac> locationBarView_;
  scoped_nsobject<AutocompleteTextFieldEditor> autocompleteTextFieldEditor_;
  id<ViewResizer> resizeDelegate_;  // weak
  scoped_nsobject<BackForwardMenuController> backMenuController_;
  scoped_nsobject<BackForwardMenuController> forwardMenuController_;
  scoped_nsobject<BrowserActionsController> browserActionsController_;

  // Lazily-instantiated model, controller, and delegate for the menu on the
  // page and wrench buttons. The wrench menu is also called the "app menu". If
  // it's visible, these will be non-null, but they are not reaped when the
  // button is hidden once it is initially shown.
  scoped_ptr<PageMenuModel> pageMenuModel_;
  scoped_nsobject<MenuController> pageMenuController_;
  scoped_ptr<ToolbarControllerInternal::MenuDelegate> menuDelegate_;
  scoped_ptr<AppMenuModel> appMenuModel_;
  scoped_nsobject<MenuController> appMenuController_;

  // Used for monitoring the optional toolbar button prefs.
  scoped_ptr<ToolbarControllerInternal::PrefObserverBridge> prefObserver_;
  // Used to position the omnibox bubble.
  scoped_ptr<BubblePositioner> bubblePositioner_;
  BooleanPrefMember showHomeButton_;
  BooleanPrefMember showPageOptionButtons_;
  BOOL hasToolbar_;  // If NO, we may have only the location bar.
  BOOL hasLocationBar_;  // If |hasToolbar_| is YES, this must also be YES.

  // We have an extra retain in the locationBar_.
  // See comments in awakeFromNib for more info.
  scoped_nsobject<AutocompleteTextField> locationBarRetainer_;

  // Tracking area for mouse enter/exit/moved in the toolbar.
  scoped_nsobject<NSTrackingArea> trackingArea_;

  // We retain/release the hover button since interaction with the
  // button may make it go away (e.g. delete menu option over a
  // bookmark button).  Thus this variable is not weak.  The
  // hoveredButton_ is required to have an NSCell that responds to
  // setMouseInside:animate:.
  NSButton* hoveredButton_;

  // The ordering is important for unit tests. If new items are added or the
  // ordering is changed, make sure to update |-toolbarViews| and the
  // corresponding enum in the unit tests.
  IBOutlet DelayedMenuButton* backButton_;
  IBOutlet DelayedMenuButton* forwardButton_;
  IBOutlet NSButton* reloadButton_;
  IBOutlet NSButton* homeButton_;
  IBOutlet NSButton* starButton_;
  IBOutlet NSButton* goButton_;
  IBOutlet MenuButton* pageButton_;
  IBOutlet MenuButton* wrenchButton_;
  IBOutlet AutocompleteTextField* locationBar_;
  IBOutlet NSView* browserActionContainerView_;
}

// Initialize the toolbar and register for command updates. The profile is
// needed for initializing the location bar. The browser is needed for
// initializing the back/forward menus.
- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate;

// Get the C++ bridge object representing the location bar for this tab.
- (LocationBar*)locationBarBridge;

// Called by the Window delegate so we can provide a custom field editor if
// needed.
// Note that this may be called for objects unrelated to the toolbar.
// returns nil if we don't want to override the custom field editor for |obj|.
- (id)customFieldEditorForObject:(id)obj;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tabForRestoring
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Called to update the loading state. Handles updating the go/stop button
// state.
- (void)setIsLoading:(BOOL)isLoading;

// Allow turning off the toolbar (but we may keep the location bar without a
// surrounding toolbar). If |toolbar| is YES, the value of |hasLocationBar| is
// ignored. This changes the behavior of other methods, like |-view|.
- (void)setHasToolbar:(BOOL)toolbar hasLocationBar:(BOOL)locBar;

// The bookmark bubble (when you click the star) needs to know where to go.
// Somewhere near the star button seems like a good start.
- (NSRect)starButtonInWindowCoordinates;

// Returns the desired toolbar height for the given compression factor.
- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight;

// Set the opacity of the divider (the line at the bottom) *if* we have a
// |ToolbarView| (0 means don't show it); no-op otherwise.
- (void)setDividerOpacity:(CGFloat)opacity;

@end

// A set of private methods used by tests, in the absence of "friends" in ObjC.
@interface ToolbarController(PrivateTestMethods)
// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews;
- (void)showOptionalHomeButton;
- (void)showOptionalPageWrenchButtons;
- (gfx::Rect)locationStackBounds;
// Return a hover button for the current event.
- (NSButton*)hoverButtonForEvent:(NSEvent*)theEvent;
- (BrowserActionsController*)browserActionsController;
@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
