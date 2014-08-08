// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller_target.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"

@class CrTrackingArea;
@class NewTabButton;
@class TabContentsController;
@class TabView;
@class TabStripDragController;
@class TabStripView;

class Browser;
class TabStripModelObserverBridge;
class TabStripModel;

namespace content {
class WebContents;
}

// The interface for the tab strip controller's delegate.
// Delegating TabStripModelObserverBridge's events (in lieu of directly
// subscribing to TabStripModelObserverBridge events, as TabStripController
// does) is necessary to guarantee a proper order of subviews layout updates,
// otherwise it might trigger unnesessary content relayout, UI flickering etc.
@protocol TabStripControllerDelegate

// Stripped down version of TabStripModelObserverBridge:selectTabWithContents.
- (void)onActivateTabWithContents:(content::WebContents*)contents;

// Stripped down version of TabStripModelObserverBridge:tabChangedWithContents.
- (void)onTabChanged:(TabStripModelObserver::TabChangeType)change
        withContents:(content::WebContents*)contents;

// Stripped down version of TabStripModelObserverBridge:tabDetachedWithContents.
- (void)onTabDetachedWithContents:(content::WebContents*)contents;

@end

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// For a full description of the design, see
// http://www.chromium.org/developers/design-documents/tab-strip-mac
@interface TabStripController :
  NSObject<TabControllerTarget,
           URLDropTargetController> {
 @private
  base::scoped_nsobject<TabStripView> tabStripView_;
  NSView* switchView_;  // weak
  base::scoped_nsobject<NSView> dragBlockingView_;  // avoid bad window server
                                                    // drags
  NewTabButton* newTabButton_;  // weak, obtained from the nib.

  // The controller that manages all the interactions of dragging tabs.
  base::scoped_nsobject<TabStripDragController> dragController_;

  // Tracks the newTabButton_ for rollovers.
  base::scoped_nsobject<CrTrackingArea> newTabTrackingArea_;
  scoped_ptr<TabStripModelObserverBridge> bridge_;
  Browser* browser_;  // weak
  TabStripModel* tabStripModel_;  // weak
  // Delegate that is informed about tab state changes.
  id<TabStripControllerDelegate> delegate_;  // weak

  // YES if the new tab button is currently displaying the hover image (if the
  // mouse is currently over the button).
  BOOL newTabButtonShowingHoverImage_;

  // Access to the TabContentsControllers (which own the parent view
  // for the toolbar and associated tab contents) given an index. Call
  // |indexFromModelIndex:| to convert a |tabStripModel_| index to a
  // |tabContentsArray_| index. Do NOT assume that the indices of
  // |tabStripModel_| and this array are identical, this is e.g. not true while
  // tabs are animating closed (closed tabs are removed from |tabStripModel_|
  // immediately, but from |tabContentsArray_| only after their close animation
  // has completed).
  base::scoped_nsobject<NSMutableArray> tabContentsArray_;
  // An array of TabControllers which manage the actual tab views. See note
  // above |tabContentsArray_|. |tabContentsArray_| and |tabArray_| always
  // contain objects belonging to the same tabs at the same indices.
  base::scoped_nsobject<NSMutableArray> tabArray_;

  // Set of TabControllers that are currently animating closed.
  base::scoped_nsobject<NSMutableSet> closingControllers_;

  // These values are only used during a drag, and override tab positioning.
  TabView* placeholderTab_;  // weak. Tab being dragged
  NSRect placeholderFrame_;  // Frame to use
  NSRect droppedTabFrame_;  // Initial frame of a dropped tab, for animation.
  // Frame targets for all the current views.
  // target frames are used because repeated requests to [NSView animator].
  // aren't coalesced, so we store frames to avoid redundant calls.
  base::scoped_nsobject<NSMutableDictionary> targetFrames_;
  NSRect newTabTargetFrame_;
  // If YES, do not show the new tab button during layout.
  BOOL forceNewTabButtonHidden_;
  // YES if we've successfully completed the initial layout. When this is
  // NO, we probably don't want to do any animation because we're just coming
  // into being.
  BOOL initialLayoutComplete_;

  // Width available for resizing the tabs (doesn't include the new tab
  // button). Used to restrict the available width when closing many tabs at
  // once to prevent them from resizing to fit the full width. If the entire
  // width should be used, this will have a value of |kUseFullAvailableWidth|.
  float availableResizeWidth_;
  // A tracking area that's the size of the tab strip used to be notified
  // when the mouse moves in the tab strip
  base::scoped_nsobject<CrTrackingArea> trackingArea_;
  TabView* hoveredTab_;  // weak. Tab that the mouse is hovering over

  // Array of subviews which are permanent (and which should never be removed),
  // such as the new-tab button, but *not* the tabs themselves.
  base::scoped_nsobject<NSMutableArray> permanentSubviews_;

  // The default favicon, so we can use one copy for all buttons.
  base::scoped_nsobject<NSImage> defaultFavicon_;

  // The amount by which to indent the tabs on the sides (to make room for the
  // red/yellow/green and incognito/fullscreen buttons).
  CGFloat leftIndentForControls_;
  CGFloat rightIndentForControls_;

  // Is the mouse currently inside the strip;
  BOOL mouseInside_;

  // Helper for performing tab selection as a result of dragging over a tab.
  scoped_ptr<HoverTabSelector> hoverTabSelector_;
}

@property(nonatomic) CGFloat leftIndentForControls;
@property(nonatomic) CGFloat rightIndentForControls;

// Initialize the controller with a view and browser that contains
// everything else we'll need. |switchView| is the view whose contents get
// "switched" every time the user switches tabs. The children of this view
// will be released, so if you want them to stay around, make sure
// you have retained them.
// |delegate| is the one listening to filtered TabStripModelObserverBridge's
// events (see TabStripControllerDelegate for more details).
- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
           browser:(Browser*)browser
          delegate:(id<TabStripControllerDelegate>)delegate;

// Returns the model behind this controller.
- (TabStripModel*)tabStripModel;

// Returns all tab views.
- (NSArray*)tabViews;

// Return the view for the currently active tab.
- (NSView*)activeTabView;

// Find the model index based on the x coordinate of the placeholder. If there
// is no placeholder, this returns the end of the tab strip. Closing tabs are
// not considered in computing the index.
- (int)indexOfPlaceholder;

// Set the frame of |tabView|, also updates the internal frame dict.
- (void)setFrame:(NSRect)frame ofTabView:(NSView*)tabView;

// Move the given tab at index |from| in this window to the location of the
// current placeholder.
- (void)moveTabFromIndex:(NSInteger)from;

// Drop a given WebContents at |modelIndex|. Used when dragging from
// another window when we don't have access to the WebContents as part of our
// strip. |frame| is in the coordinate system of the tab strip view and
// represents where the user dropped the new tab so it can be animated into its
// correct location when the tab is added to the model. If the tab was pinned in
// its previous window, setting |pinned| to YES will propagate that state to the
// new window. Mini-tabs are either app or pinned tabs; the app state is stored
// by the |contents|, but the |pinned| state is the caller's responsibility.
// Setting |activate| to YES will make the new tab active.
- (void)dropWebContents:(content::WebContents*)contents
                atIndex:(int)modelIndex
              withFrame:(NSRect)frame
            asPinnedTab:(BOOL)pinned
               activate:(BOOL)activate;

// Returns the index of the subview |view|. Returns -1 if not present. Takes
// closing tabs into account such that this index will correctly match the tab
// model. If |view| is in the process of closing, returns -1, as closing tabs
// are no longer in the model.
- (NSInteger)modelIndexForTabView:(NSView*)view;

// Returns all selected tab views.
- (NSArray*)selectedViews;

// Return the view at a given index.
- (NSView*)viewAtIndex:(NSUInteger)index;

// Return the number of tab views in the tab strip. It's same as number of tabs
// in the model, except when a tab is closing, which will be counted in views
// count, but no longer in the model.
- (NSUInteger)viewsCount;

// Set the placeholder for a dragged tab, allowing the |frame| to be specified.
// This causes this tab to be rendered in an arbitrary position.
- (void)insertPlaceholderForTab:(TabView*)tab frame:(NSRect)frame;

// Returns whether a tab is being dragged within the tab strip.
- (BOOL)isDragSessionActive;

// Returns whether or not |tab| can still be fully seen in the tab strip or if
// its current position would cause it be obscured by things such as the edge
// of the window or the window decorations. Returns YES only if the entire tab
// is visible.
- (BOOL)isTabFullyVisible:(TabView*)tab;

// Show or hide the new tab button. The button is hidden immediately, but
// waits until the next call to |-layoutTabs| to show it again.
- (void)showNewTabButton:(BOOL)show;

// Force the tabs to rearrange themselves to reflect the current model.
- (void)layoutTabs;
- (void)layoutTabsWithoutAnimation;

// Are we in rapid (tab) closure mode? I.e., is a full layout deferred (while
// the user closes tabs)? Needed to overcome missing clicks during rapid tab
// closure.
- (BOOL)inRapidClosureMode;

// Returns YES if the user is allowed to drag tabs on the strip at this moment.
// For example, this returns NO if there are any pending tab close animtations.
- (BOOL)tabDraggingAllowed;

// Default height for tabs.
+ (CGFloat)defaultTabHeight;

// Default indentation for tabs (see |leftIndentForControls_|).
+ (CGFloat)defaultLeftIndentForControls;

// Returns the currently active TabContentsController.
- (TabContentsController*)activeTabContentsController;

@end

@interface TabStripController(TestingAPI)
- (void)setTabTitle:(TabController*)tab
       withContents:(content::WebContents*)contents;
@end

// Returns the parent view to use when showing a sheet for a given web contents.
NSView* GetSheetParentViewForWebContents(content::WebContents* web_contents);

// Returns the bounds to use when showing a sheet for a given parent view. This
// returns a rect in window coordinates.
NSRect GetSheetParentBoundsForParentView(NSView* view);

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_STRIP_CONTROLLER_H_
