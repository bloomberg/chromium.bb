// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/command_observer_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/delayedmenu_button.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/common/pref_member.h"

class AutocompletePopupPositioner;
@class AutocompleteTextField;
@class AutocompleteTextFieldEditor;
@class DelayedMenuButton;
@class BackForwardMenuController;
class Browser;
class CommandUpdater;
class LocationBar;
class LocationBarViewMac;
namespace ToolbarControllerInternal {
class PrefObserverBridge;
}
class Profile;
class TabContents;
class ToolbarModel;
class ToolbarView;

// A controller for the toolbar in the browser window. Manages
// updating the state for location bar and back/fwd/reload/go buttons.
// Manages the bookmark bar and it's position in the window relative to
// the web content view.

@interface ToolbarController :
    NSViewController<CommandObserverProtocol, ViewResizer> {
 @private
  ToolbarModel* toolbarModel_;  // weak, one per window
  CommandUpdater* commands_;  // weak, one per window
  Profile* profile_;  // weak, one per window
  Browser* browser_;  // weak, one per window
  scoped_ptr<CommandObserverBridge> commandObserver_;
  scoped_ptr<LocationBarViewMac> locationBarView_;
  scoped_nsobject<AutocompleteTextFieldEditor> autocompleteTextFieldEditor_;
  scoped_nsobject<BookmarkBarController> bookmarkBarController_;
  id<ViewResizer> resizeDelegate_;  // weak
  id<BookmarkURLOpener> bookmarkBarDelegate_;  // weak
  scoped_nsobject<BackForwardMenuController> backMenuController_;
  scoped_nsobject<BackForwardMenuController> forwardMenuController_;

  // Used for monitoring the optional toolbar button prefs.
  scoped_ptr<ToolbarControllerInternal::PrefObserverBridge> prefObserver_;
  // Used to positioner the omnibox popup view.
  scoped_ptr<AutocompletePopupPositioner> popupPositioner_;
  BooleanPrefMember showHomeButton_;
  BooleanPrefMember showPageOptionButtons_;
  BOOL hasToolbar_;  // if NO, we only have the location bar.

  // We have an extra retain in the locationBar_.
  // See comments in awakeFromNib for more info.
  scoped_nsobject<AutocompleteTextField> locationBarRetainer_;

  // Tracking area for mouse enter/exit/moved in the toolbar.
  scoped_nsobject<NSTrackingArea> trackingArea_;
  NSButton* hoveredButton_;  // weak. Button under the mouse cursor.

  IBOutlet NSMenu* pageMenu_;
  IBOutlet NSMenu* wrenchMenu_;

  // The ordering is important for unit tests. If new items are added or the
  // ordering is changed, make sure to update |-toolbarViews| and the
  // corresponding enum in the unit tests.
  IBOutlet DelayedMenuButton* backButton_;
  IBOutlet DelayedMenuButton* forwardButton_;
  IBOutlet NSButton* reloadButton_;
  IBOutlet NSButton* homeButton_;
  IBOutlet NSButton* starButton_;
  IBOutlet NSButton* goButton_;
  IBOutlet NSButton* pageButton_;
  IBOutlet NSButton* wrenchButton_;
  IBOutlet AutocompleteTextField* locationBar_;
}

// Initialize the toolbar and register for command updates. The profile is
// needed for initializing the location bar. The browser is needed for
// initializing the back/forward menus.
- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate
   bookmarkDelegate:(id<BookmarkURLOpener>)delegate;

// Get the C++ bridge object representing the location bar for this tab.
- (LocationBar*)locationBar;

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

// Allow turning off the toolbar (but we keep the location bar
// around).  This changes the behavior of other methods, like
// [self view].
- (void)setHasToolbar:(BOOL)toolbar;

// Return the bookmark bar controller.
- (BookmarkBarController*)bookmarkBarController;

// Actions for the optional menu buttons for the page and wrench menus. These
// will show a menu while the mouse is down.
- (IBAction)showPageMenu:(id)sender;
- (IBAction)showWrenchMenu:(id)sender;

// The bookmark bubble (when you click the star) needs to know where to go.
// Somewhere near the star button seems like a good start.
- (NSRect)starButtonInWindowCoordinates;

@end

// A set of private methods used by tests, in the absence of "friends" in ObjC.
@interface ToolbarController(PrivateTestMethods)
// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews;
- (void)showOptionalHomeButton;
- (void)showOptionalPageWrenchButtons;
- (gfx::Rect)autocompletePopupPosition;
@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
