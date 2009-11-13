// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_controller.h"

#import <QuartzCore/QuartzCore.h>

#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/constrained_window_mac.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_contents_controller.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/throbber_view.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

NSString* const kTabStripNumberOfTabsChanged = @"kTabStripNumberOfTabsChanged";

// A value to indicate tab layout should use the full available width of the
// view.
static const float kUseFullAvailableWidth = -1.0;

// Left-side indent for tab layout so tabs don't overlap with the window
// controls.
static const float kIndentLeavingSpaceForControls = 64.0;

// Time (in seconds) in which tabs animate to their final position.
static const NSTimeInterval kAnimationDuration = 0.2;

namespace {

// Helper class for doing NSAnimationContext calls that takes a bool to disable
// all the work.  Useful for code that wants to conditionally animate.
class ScopedNSAnimationContextGroup {
 public:
  explicit ScopedNSAnimationContextGroup(bool animate)
      : animate_(animate) {
    if (animate_) {
      [NSAnimationContext beginGrouping];
    }
  }

  ~ScopedNSAnimationContextGroup() {
    if (animate_) {
      [NSAnimationContext endGrouping];
    }
  }

  void SetCurrentContextDuration(NSTimeInterval duration) {
    if (animate_) {
      [[NSAnimationContext currentContext] gtm_setDuration:duration];
    }
  }

  void SetCurrentContextShortestDuration() {
    if (animate_) {
      // The minimum representable time interval.  This used to stop an
      // in-progress animation as quickly as possible.
      const NSTimeInterval kMinimumTimeInterval =
          std::numeric_limits<NSTimeInterval>::min();
      // Directly set the duration to be short, avoiding the Steve slowmotion
      // ettect the gtm_setDuration: provides.
      [[NSAnimationContext currentContext] setDuration:kMinimumTimeInterval];
    }
  }

private:
  bool animate_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNSAnimationContextGroup);
};

}  // namespace

@interface TabStripController (Private)
- (void)installTrackingArea;
- (void)addSubviewToPermanentList:(NSView*)aView;
- (void)regenerateSubviewList;
- (NSInteger)indexForContentsView:(NSView*)view;
- (void)updateFavIconForContents:(TabContents*)contents
                         atIndex:(NSInteger)index;
- (void)layoutTabsWithAnimation:(BOOL)animate
             regenerateSubviews:(BOOL)doUpdate;
- (void)animationDidStopForController:(TabController*)controller
                             finished:(BOOL)finished;
- (NSInteger)indexFromModelIndex:(NSInteger)index;
- (void)mouseMoved:(NSEvent*)event;
- (void)setTabTrackingAreasEnabled:(BOOL)enabled;
@end

// A simple view class that prevents the Window Server from dragging the area
// behind tabs. Sometimes core animation confuses it. Unfortunately, it can also
// falsely pick up clicks during rapid tab closure, so we have to account for
// that.
@interface TabStripControllerDragBlockingView : NSView {
  TabStripController* controller_;  // weak; owns us
}

- (id)initWithFrame:(NSRect)frameRect
         controller:(TabStripController*)controller;
@end

@implementation TabStripControllerDragBlockingView
- (BOOL)mouseDownCanMoveWindow {return NO;}
- (void)drawRect:(NSRect)rect {}

- (id)initWithFrame:(NSRect)frameRect
         controller:(TabStripController*)controller {
  if ((self = [super initWithFrame:frameRect]))
    controller_ = controller;
  return self;
}

// In "rapid tab closure" mode (i.e., the user is clicking close tab buttons in
// rapid succession), the animations confuse Cocoa's hit testing (which appears
// to use cached results, among other tricks), so this view can somehow end up
// getting a mouse down event. Thus we do an explicit hit test during rapid tab
// closure, and if we find that we got a mouse down we shouldn't have, we send
// it off to the appropriate view.
- (void)mouseDown:(NSEvent*)event {
  if ([controller_ inRapidClosureMode]) {
    NSView* superview = [self superview];
    NSPoint hitLocation =
        [[superview superview] convertPoint:[event locationInWindow]
                                   fromView:nil];
    NSView* hitView = [superview hitTest:hitLocation];
    if (hitView != self) {
      [hitView mouseDown:event];
      return;
    }
  }
  [super mouseDown:event];
}
@end

#pragma mark -

// A delegate, owned by the CAAnimation system, that is alerted when the
// animation to close a tab is completed. Calls back to the given tab strip
// to let it know that |controller_| is ready to be removed from the model.
// Since we only maintain weak references, the tab strip must call -invalidate:
// to prevent the use of dangling pointers.
@interface TabCloseAnimationDelegate : NSObject {
 @private
  TabStripController* strip_;  // weak; owns us indirectly
  TabController* controller_;  // weak
}

// Will tell |strip| when the animation for |controller|'s view has completed.
// These should not be nil, and will not be retained.
- (id)initWithTabStrip:(TabStripController*)strip
         tabController:(TabController*)controller;

// Invalidates this object so that no further calls will be made to
// |strip_|.  This should be called when |strip_| is released, to
// prevent attempts to call into the released object.
- (void)invalidate;

// CAAnimation delegate method
- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished;

@end

@implementation TabCloseAnimationDelegate

- (id)initWithTabStrip:(TabStripController*)strip
         tabController:(TabController*)controller {
  if ((self == [super init])) {
    DCHECK(strip && controller);
    strip_ = strip;
    controller_ = controller;
  }
  return self;
}

- (void)invalidate {
  strip_ = nil;
  controller_ = nil;
}

- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished {
  [strip_ animationDidStopForController:controller_ finished:finished];
}

@end

#pragma mark -

// TODO(pinkerton): document tab layout, placeholders, tab dragging on
// dev.chromium.org

// In general, there is a one-to-one correspondence between TabControllers,
// TabViews, TabContentsControllers, and the TabContents in the TabStripModel.
// In the steady-state, the indices line up so an index coming from the model
// is directly mapped to the same index in the parallel arrays holding our
// views and controllers. This is also true when new tabs are created (even
// though there is a small period of animation) because the tab is present
// in the model while the TabView is animating into place. As a result, nothing
// special need be done to handle "new tab" animation.
//
// This all goes out the window with the "close tab" animation. The animation
// kicks off in |-tabDetachedWithContents:atIndex:| with the notifiation that
// the tab has been removed from the model. The simplest solution at this
// point would be to remove the views and controllers as well, however once
// the TabView is removed from the view list, the tab z-order code takes care of
// removing it from the tab strip and we'll get no animation. That means if
// there is to be any visible animation, the TabView needs to stay around until
// its animation is complete. In order to maintain consistency among the
// internal parallel arrays, this means all structures are kept around until
// the animation completes. At this point, though, the model and our internal
// structures are out of sync: the indices no longer line up. As a result,
// there is a concept of a "model index" which represents an index valid in
// the TabStripModel. During steady-state, the "model index" is just the same
// index as our parallel arrays (as above), but during tab close animations,
// it is different, offset by the number of tabs preceding the index which
// are undergoing tab closing animation. As a result, the caller needs to be
// careful to use the available conversion routines when accessing the internal
// parallel arrays (e.g., -indexFromModelIndex:). Care also needs to be taken
// during tab layout to ignore closing tabs in the total width calculations and
// in individual tab positioning (to avoid moving them right back to where they
// were).
//
// In order to prevent actions being taken on tabs which are closing, the tab
// itself gets marked as such so it no longer will send back its select action
// or allow itself to be dragged. In addition, drags on the tab strip as a
// whole are disabled while there are tabs closing.

@implementation TabStripController

- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
           browser:(Browser*)browser {
  DCHECK(view && switchView && browser);
  if ((self = [super init])) {
    tabView_.reset([view retain]);
    switchView_ = switchView;
    browser_ = browser;
    tabModel_ = browser_->tabstrip_model();
    bridge_.reset(new TabStripModelObserverBridge(tabModel_, self));
    tabContentsArray_.reset([[NSMutableArray alloc] init]);
    tabArray_.reset([[NSMutableArray alloc] init]);
    permanentSubviews_.reset([[NSMutableArray alloc] init]);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    defaultFavIcon_.reset([rb.GetNSImageNamed(IDR_DEFAULT_FAVICON) retain]);

    // Take the only child view present in the nib as the new tab button. For
    // some reason, if the view is present in the nib apriori, it draws
    // correctly. If we create it in code and add it to the tab view, it draws
    // with all sorts of crazy artifacts.
    newTabButton_ = [[tabView_ subviews] objectAtIndex:0];
    DCHECK([newTabButton_ isKindOfClass:[NSButton class]]);
    [self addSubviewToPermanentList:newTabButton_];
    [newTabButton_ setTarget:nil];
    [newTabButton_ setAction:@selector(commandDispatch:)];
    [newTabButton_ setTag:IDC_NEW_TAB];
    newTabTrackingArea_.reset(
        [[NSTrackingArea alloc] initWithRect:[newTabButton_ bounds]
                                     options:(NSTrackingMouseEnteredAndExited |
                                              NSTrackingActiveAlways)
                                       owner:self
                                    userInfo:nil]);
    [newTabButton_ addTrackingArea:newTabTrackingArea_.get()];
    targetFrames_.reset([[NSMutableDictionary alloc] init]);
    dragBlockingView_.reset(
        [[TabStripControllerDragBlockingView alloc] initWithFrame:NSZeroRect
                                                       controller:self]);
    [self addSubviewToPermanentList:dragBlockingView_];
    newTabTargetFrame_ = NSMakeRect(0, 0, 0, 0);
    availableResizeWidth_ = kUseFullAvailableWidth;

    closingControllers_.reset([[NSMutableSet alloc] init]);

    // Install the permanent subviews.
    [self regenerateSubviewList];

    // Watch for notifications that the tab strip view has changed size so
    // we can tell it to layout for the new size.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(tabViewFrameChanged:)
               name:NSViewFrameDidChangeNotification
             object:tabView_];

    trackingArea_.reset([[NSTrackingArea alloc]
        initWithRect:NSZeroRect  // Ignored by NSTrackingInVisibleRect
             options:NSTrackingMouseEnteredAndExited |
                     NSTrackingMouseMoved |
                     NSTrackingActiveAlways |
                     NSTrackingInVisibleRect
               owner:self
            userInfo:nil]);
    [tabView_ addTrackingArea:trackingArea_.get()];
  }
  return self;
}

- (void)dealloc {
  if (trackingArea_.get())
    [tabView_ removeTrackingArea:trackingArea_.get()];
  [newTabButton_ removeTrackingArea:newTabTrackingArea_.get()];
  // Invalidate all closing animations so they don't call back to us after
  // we're gone.
  for (TabController* controller in closingControllers_.get()) {
    NSView* view = [controller view];
    [[[view animationForKey:@"frameOrigin"] delegate] invalidate];
  }
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

+ (CGFloat)defaultTabHeight {
  return 24.0;
}

// Finds the TabContentsController associated with the given index into the tab
// model and swaps out the sole child of the contentArea to display its
// contents.
- (void)swapInTabAtIndex:(NSInteger)modelIndex {
  DCHECK(modelIndex >= 0 && modelIndex < tabModel_->count());
  NSInteger index = [self indexFromModelIndex:modelIndex];
  TabContentsController* controller = [tabContentsArray_ objectAtIndex:index];

  // Resize the new view to fit the window. Calling |view| may lazily
  // instantiate the TabContentsController from the nib. Until we call
  // |-ensureContentsVisible|, the controller doesn't install the RWHVMac into
  // the view hierarchy. This is in order to avoid sending the renderer a
  // spurious default size loaded from the nib during the call to |-view|.
  NSView* newView = [controller view];
  NSRect frame = [switchView_ bounds];
  [newView setFrame:frame];
  [controller ensureContentsVisible];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of |switchView_| because we're the one who put it there. There
  // may not be any children in the case of a tab that's been closed, in
  // which case there's no swapping going on.
  NSArray* subviews = [switchView_ subviews];
  if ([subviews count]) {
    NSView* oldView = [subviews objectAtIndex:0];
    [switchView_ replaceSubview:oldView with:newView];
  } else {
    [switchView_ addSubview:newView];
  }

  // Make sure the new tabs's sheets are visible (necessary when a background
  // tab opened a sheet while it was in the background and now becomes active).
  TabContents* newTab = tabModel_->GetTabContentsAt(modelIndex);
  DCHECK(newTab);
  if (newTab) {
    TabContents::ConstrainedWindowList::iterator it, end;
    it = newTab->constrained_window_begin();
    end = newTab->constrained_window_end();

    // GTMWindowSheetController supports only one sheet at a time.
    if (it != end) {
      NSWindowController* controller = [[newView window] windowController];
      DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
      ConstrainedWindow* constrainedWindow = *it;
      static_cast<ConstrainedWindowMac*>(constrainedWindow)->Realize(
          static_cast<BrowserWindowController*>(controller));
    }
  }

  // Tell per-tab sheet manager about currently selected tab.
  if (sheetController_.get()) {
    [sheetController_ setActiveView:newView];
  }
}

// Create a new tab view and set its cell correctly so it draws the way we want
// it to. It will be sized and positioned by |-layoutTabs| so there's no need to
// set the frame here. This also creates the view as hidden, it will be
// shown during layout.
- (TabController*)newTab {
  TabController* controller = [[[TabController alloc] init] autorelease];
  [controller setTarget:self];
  [controller setAction:@selector(selectTab:)];
  [[controller view] setHidden:YES];

  return controller;
}

// Returns the number of open tabs in the tab strip. This is the number
// of TabControllers we know about (as there's a 1-to-1 mapping from these
// controllers to a tab) less the number of closing tabs.
- (NSInteger)numberOfTabViews {
  NSInteger number = [tabArray_ count] - [closingControllers_ count];
  DCHECK(number >= 0);
  return number;
}

// Given an index into the tab model, returns the index into the tab controller
// array accounting for tabs that are currently closing. For example, if there
// are two tabs in the process of closing before |index|, this returns
// |index| + 2. If there are no closing tabs, this will return |index|.
- (NSInteger)indexFromModelIndex:(NSInteger)index {
  NSInteger i = 0;
  for (TabController* controller in tabArray_.get()) {
    if ([closingControllers_ containsObject:controller]) {
      DCHECK([(TabView*)[controller view] isClosing]);
      ++index;
    }
    if (i == index)  // No need to check anything after, it has no effect.
      break;
    ++i;
  }
  return index;
}


// Returns the index of the subview |view|. Returns -1 if not present. Takes
// closing tabs into account such that this index will correctly match the tab
// model. If |view| is in the process of closing, returns -1, as closing tabs
// are no longer in the model.
- (NSInteger)modelIndexForTabView:(NSView*)view {
  NSInteger index = 0;
  for (TabController* current in tabArray_.get()) {
    // If |current| is closing, skip it.
    if ([closingControllers_ containsObject:current])
      continue;
    else if ([current view] == view)
      return index;
    ++index;
  }
  return -1;
}

// Returns the index of the contents subview |view|. Returns -1 if not present.
// Takes closing tabs into account such that this index will correctly match the
// tab model. If |view| is in the process of closing, returns -1, as closing
// tabs are no longer in the model.
- (NSInteger)modelIndexForContentsView:(NSView*)view {
  NSInteger index = 0;
  NSInteger i = 0;
  for (TabContentsController* current in tabContentsArray_.get()) {
    // If the TabController corresponding to |current| is closing, skip it.
    TabController* controller = [tabArray_ objectAtIndex:i];
    if ([closingControllers_ containsObject:controller]) {
      ++i;
      continue;
    } else if ([current view] == view) {
      return index;
    }
    ++index;
    ++i;
  }
  return -1;
}


// Returns the view at the given index, using the array of TabControllers to
// get the associated view. Returns nil if out of range.
- (NSView*)viewAtIndex:(NSUInteger)index {
  if (index >= [tabArray_ count])
    return NULL;
  return [[tabArray_ objectAtIndex:index] view];
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  DCHECK([sender isKindOfClass:[NSView class]]);
  int index = [self modelIndexForTabView:sender];
  if (tabModel_->ContainsIndex(index))
    tabModel_->SelectTabContentsAt(index, true);
}

// Called when the user closes a tab. Asks the model to close the tab. |sender|
// is the TabView that is potentially going away.
- (void)closeTab:(id)sender {
  DCHECK([sender isKindOfClass:[TabView class]]);
  if ([hoveredTab_ isEqual:sender]) {
    hoveredTab_ = nil;
  }

  NSInteger index = [self modelIndexForTabView:sender];
  if (!tabModel_->ContainsIndex(index))
    return;

  TabContents* contents = tabModel_->GetTabContentsAt(index);
  if (contents)
    UserMetrics::RecordAction(L"CloseTab_Mouse", contents->profile());
  const NSInteger numberOfTabViews = [self numberOfTabViews];
  if (numberOfTabViews > 1) {
    bool isClosingLastTab = index == numberOfTabViews - 1;
    if (!isClosingLastTab) {
      // Limit the width available for laying out tabs so that tabs are not
      // resized until a later time (when the mouse leaves the tab strip).
      // TODO(pinkerton): re-visit when handling tab overflow.
      NSView* penultimateTab = [self viewAtIndex:numberOfTabViews - 2];
      availableResizeWidth_ = NSMaxX([penultimateTab frame]);
    } else {
      // If the rightmost tab is closed, change the available width so that
      // another tab's close button lands below the cursor (assuming the tabs
      // are currently below their maximum width and can grow).
      NSView* lastTab = [self viewAtIndex:numberOfTabViews - 1];
      availableResizeWidth_ = NSMaxX([lastTab frame]);
    }
    tabModel_->CloseTabContentsAt(index);
  } else {
    // Use the standard window close if this is the last tab
    // this prevents the tab from being removed from the model until after
    // the window dissapears
    [[tabView_ window] performClose:nil];
  }
}

// Dispatch context menu commands for the given tab controller.
- (void)commandDispatch:(TabStripModel::ContextMenuCommand)command
          forController:(TabController*)controller {
  int index = [self modelIndexForTabView:[controller view]];
  if (tabModel_->ContainsIndex(index))
    tabModel_->ExecuteContextMenuCommand(index, command);
}

// Returns YES if the specificed command should be enabled for the given
// controller.
- (BOOL)isCommandEnabled:(TabStripModel::ContextMenuCommand)command
           forController:(TabController*)controller {
  int index = [self modelIndexForTabView:[controller view]];
  if (!tabModel_->ContainsIndex(index))
    return NO;
  return tabModel_->IsContextMenuCommandEnabled(index, command) ? YES : NO;
}

- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                  yStretchiness:(CGFloat)yStretchiness {
  placeholderTab_ = tab;
  placeholderFrame_ = frame;
  placeholderStretchiness_ = yStretchiness;
  [self layoutTabsWithAnimation:initialLayoutComplete_ regenerateSubviews:NO];
}

- (BOOL)isTabFullyVisible:(TabView*)tab {
  NSRect frame = [tab frame];
  return NSMinX(frame) >= kIndentLeavingSpaceForControls &&
      NSMaxX(frame) <= NSMaxX([tabView_ frame]);
}

- (void)showNewTabButton:(BOOL)show {
  forceNewTabButtonHidden_ = show ? NO : YES;
  if (forceNewTabButtonHidden_)
    [newTabButton_ setHidden:YES];
}

// Lay out all tabs in the order of their TabContentsControllers, which matches
// the ordering in the TabStripModel. This call isn't that expensive, though
// it is O(n) in the number of tabs. Tabs will animate to their new position
// if the window is visible and |animate| is YES.
// TODO(pinkerton): Handle drag placeholders via proxy objects, perhaps a
// subclass of TabContentsController with everything stubbed out or by
// abstracting a base class interface.
// TODO(pinkerton): Note this doesn't do too well when the number of min-sized
// tabs would cause an overflow.
- (void)layoutTabsWithAnimation:(BOOL)animate
             regenerateSubviews:(BOOL)doUpdate {
  DCHECK([NSThread isMainThread]);
  if (![tabArray_ count])
    return;

  const float kTabOverlap = 20.0;
  const float kNewTabButtonOffset = 8.0;
  const float kMaxTabWidth = [TabController maxTabWidth];
  const float kMinTabWidth = [TabController minTabWidth];
  const float kMinSelectedTabWidth = [TabController minSelectedTabWidth];

  NSRect enclosingRect = NSZeroRect;
  ScopedNSAnimationContextGroup mainAnimationGroup(animate);
  mainAnimationGroup.SetCurrentContextDuration(kAnimationDuration);

  // Update the current subviews and their z-order if requested.
  if (doUpdate)
    [self regenerateSubviewList];

  // Compute the base width of tabs given how much room we're allowed. We
  // may not be able to use the entire width if the user is quickly closing
  // tabs.
  float availableWidth = 0;
  if ([self inRapidClosureMode]) {
    availableWidth = availableResizeWidth_;
  } else {
    availableWidth = NSWidth([tabView_ frame]);
    availableWidth -= NSWidth([newTabButton_ frame]) + kNewTabButtonOffset;
  }
  availableWidth -= kIndentLeavingSpaceForControls;

  // Only count the number of tabs that are not in the process of closing.
  const NSUInteger numberOfOpenTabs =
    [tabContentsArray_ count] - [closingControllers_ count];

  // Add back in the amount we "get back" from the tabs overlapping.
  availableWidth += (numberOfOpenTabs - 1) * kTabOverlap;
  const float baseTabWidth =
      MAX(MIN(availableWidth / numberOfOpenTabs,
              kMaxTabWidth),
          kMinTabWidth);

  CGFloat minX = NSMinX(placeholderFrame_);
  BOOL visible = [[tabView_ window] isVisible];

  float offset = kIndentLeavingSpaceForControls;
  NSUInteger i = 0;
  bool hasPlaceholderGap = false;
  for (TabController* tab in tabArray_.get()) {
    // Ignore a tab that is going through a close animation.
    if ([closingControllers_ containsObject:tab])
      continue;

    BOOL isPlaceholder = [[tab view] isEqual:placeholderTab_];
    NSRect tabFrame = [[tab view] frame];
    tabFrame.size.height = [[self class] defaultTabHeight] + 1;
    tabFrame.origin.y = 0;
    tabFrame.origin.x = offset;

    // If the tab is hidden, we consider it a new tab. We make it visible
    // and animate it in.
    BOOL newTab = [[tab view] isHidden];
    if (newTab) {
      [[tab view] setHidden:NO];
    }

    if (isPlaceholder) {
      // Move the current tab to the correct location instantly.
      // We need a duration or else it doesn't cancel an inflight animation.
      ScopedNSAnimationContextGroup localAnimationGroup(animate);
      localAnimationGroup.SetCurrentContextShortestDuration();
      tabFrame.origin.x = placeholderFrame_.origin.x;
      // TODO(alcor): reenable this
      //tabFrame.size.height += 10.0 * placeholderStretchiness_;
      id target = animate ? [[tab view] animator] : [tab view];
      [target setFrame:tabFrame];

      // Store the frame by identifier to aviod redundant calls to animator.
      NSValue* identifier = [NSValue valueWithPointer:[tab view]];
      [targetFrames_ setObject:[NSValue valueWithRect:tabFrame]
                        forKey:identifier];
      continue;
    } else {
      // If our left edge is to the left of the placeholder's left, but our mid
      // is to the right of it we should slide over to make space for it.
      if (placeholderTab_ && !hasPlaceholderGap && NSMidX(tabFrame) > minX) {
        hasPlaceholderGap = true;
        offset += NSWidth(placeholderFrame_);
        offset -= kTabOverlap;
        tabFrame.origin.x = offset;
      }

      // Set the width. Selected tabs are slightly wider when things get
      // really small and thus we enforce a different minimum width.
      tabFrame.size.width =
          [tab selected] ? MAX(baseTabWidth, kMinSelectedTabWidth) :
                           baseTabWidth;

      // Animate a new tab in by putting it below the horizon unless
      // told to put it in a specific location (ie, from a drop).
      if (newTab && visible && animate) {
        if (NSEqualRects(droppedTabFrame_, NSZeroRect)) {
          [[tab view] setFrame:NSOffsetRect(tabFrame, 0, -NSHeight(tabFrame))];
        } else {
          [[tab view] setFrame:droppedTabFrame_];
          droppedTabFrame_ = NSZeroRect;
        }
      }

      // Check the frame by identifier to avoid redundant calls to animator.
      id frameTarget = visible && animate ? [[tab view] animator] : [tab view];
      NSValue* identifier = [NSValue valueWithPointer:[tab view]];
      NSValue* oldTargetValue = [targetFrames_ objectForKey:identifier];
      if (!oldTargetValue ||
          !NSEqualRects([oldTargetValue rectValue], tabFrame)) {
        [frameTarget setFrame:tabFrame];
        [targetFrames_ setObject:[NSValue valueWithRect:tabFrame]
                          forKey:identifier];
      }

      enclosingRect = NSUnionRect(tabFrame, enclosingRect);
    }

    offset += NSWidth(tabFrame);
    offset -= kTabOverlap;
    i++;
  }

  // Hide the new tab button if we're explicitly told to. It may already
  // be hidden, doing it again doesn't hurt. Otherwise position it
  // appropriately, showing it if necessary.
  if (forceNewTabButtonHidden_) {
    [newTabButton_ setHidden:YES];
  } else {
    NSRect newTabNewFrame = [newTabButton_ frame];
    // We've already ensured there's enough space for the new tab button
    // so we don't have to check it against the available width. We do need
    // to make sure we put it after any placeholder.
    newTabNewFrame.origin = NSMakePoint(offset, 0);
    newTabNewFrame.origin.x = MAX(newTabNewFrame.origin.x,
                                  NSMaxX(placeholderFrame_)) +
                                      kNewTabButtonOffset;
    if ([tabContentsArray_ count])
      [newTabButton_ setHidden:NO];

    if (!NSEqualRects(newTabTargetFrame_, newTabNewFrame)) {
      // Set the new tab button image correctly based on where the cursor is.
      NSWindow* window = [tabView_ window];
      NSPoint currentMouse = [window mouseLocationOutsideOfEventStream];
      currentMouse = [tabView_ convertPoint:currentMouse fromView:nil];
      NSString* imageName = nil;
      if (NSPointInRect(currentMouse, newTabNewFrame)) {
        imageName = @"newtab_h";
      } else {
        imageName = @"newtab";
      }
      [newTabButton_ setImage:nsimage_cache::ImageNamed(imageName)];

      // Move the new tab button into place. We want to animate the new tab
      // button if it's moving to the left (closing a tab), but not when it's
      // moving to the right (inserting a new tab). If moving right, we need
      // to use a very small duration to make sure we cancel any in-flight
      // animation to the left.
      if (visible && animate) {
        ScopedNSAnimationContextGroup localAnimationGroup(true);
        BOOL movingLeft = NSMinX(newTabNewFrame) < NSMinX(newTabTargetFrame_);
        if (!movingLeft) {
          localAnimationGroup.SetCurrentContextShortestDuration();
        }
        [[newTabButton_ animator] setFrame:newTabNewFrame];
        newTabTargetFrame_ = newTabNewFrame;
      } else {
        [newTabButton_ setFrame:newTabNewFrame];
        newTabTargetFrame_ = newTabNewFrame;
      }
    }
  }

  [dragBlockingView_ setFrame:enclosingRect];

  // Mark that we've successfully completed layout of at least one tab.
  initialLayoutComplete_ = YES;
}

// When we're told to layout from the public API we usually want to animate,
// except when it's the first time.
- (void)layoutTabs {
  [self layoutTabsWithAnimation:initialLayoutComplete_ regenerateSubviews:YES];
}

// Handles setting the title of the tab based on the given |contents|. Uses
// a canned string if |contents| is NULL.
- (void)setTabTitle:(NSViewController*)tab withContents:(TabContents*)contents {
  NSString* titleString = nil;
  if (contents)
    titleString = base::SysUTF16ToNSString(contents->GetTitle());
  if (![titleString length]) {
    titleString =
      base::SysWideToNSString(
          l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
  }
  [tab setTitle:titleString];
}

// Called when a notification is received from the model to insert a new tab
// at |index|.
- (void)insertTabWithContents:(TabContents*)contents
                      atIndex:(NSInteger)modelIndex
                 inForeground:(bool)inForeground {
  DCHECK(contents);
  DCHECK(modelIndex == TabStripModel::kNoTab ||
         tabModel_->ContainsIndex(modelIndex));

  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  // Make a new tab. Load the contents of this tab from the nib and associate
  // the new controller with |contents| so it can be looked up later.
  TabContentsController* contentsController =
      [[[TabContentsController alloc] initWithNibName:@"TabContents"
                                             contents:contents]
          autorelease];
  [tabContentsArray_ insertObject:contentsController atIndex:index];

  // Make a new tab and add it to the strip. Keep track of its controller.
  TabController* newController = [self newTab];
  [tabArray_ insertObject:newController atIndex:index];
  NSView* newView = [newController view];

  // Set the originating frame to just below the strip so that it animates
  // upwards as it's being initially layed out. Oddly, this works while doing
  // something similar in |-layoutTabs| confuses the window server.
  // TODO(pinkerton): I'm not happy with this animiation either, but it's
  // a little better that just sliding over (maybe?).
  [newView setFrame:NSOffsetRect([newView frame],
                                 0, -[[self class] defaultTabHeight])];

  [self setTabTitle:newController withContents:contents];

  // If a tab is being inserted, we can again use the entire tab strip width
  // for layout.
  availableResizeWidth_ = kUseFullAvailableWidth;

  // We don't need to call |-layoutTabs| if the tab will be in the foreground
  // because it will get called when the new tab is selected by the tab model.
  // Whenever |-layoutTabs| is called, it'll also add the new subview.
  if (!inForeground) {
    [self layoutTabs];
  }

  // During normal loading, we won't yet have a favicon and we'll get
  // subsequent state change notifications to show the throbber, but when we're
  // dragging a tab out into a new window, we have to put the tab's favicon
  // into the right state up front as we won't be told to do it from anywhere
  // else.
  [self updateFavIconForContents:contents atIndex:index];

  // Send a broadcast that the number of tabs have changed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabStripNumberOfTabsChanged
                    object:self];
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)modelIndex
                  userGesture:(bool)wasUserGesture {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  // De-select all other tabs and select the new tab.
  int i = 0;
  for (TabController* current in tabArray_.get()) {
    [current setSelected:(i == index) ? YES : NO];
    ++i;
  }

  // Tell the new tab contents it is about to become the selected tab. Here it
  // can do things like make sure the toolbar is up to date.
  TabContentsController* newController =
      [tabContentsArray_ objectAtIndex:index];
  [newController willBecomeSelectedTab];

  // Relayout for new tabs and to let the selected tab grow to be larger in
  // size than surrounding tabs if the user has many. This also raises the
  // selected tab to the top.
  [self layoutTabs];

  if (oldContents) {
    oldContents->view()->StoreFocus();
    oldContents->WasHidden();
  }

  // Swap in the contents for the new tab.
  [self swapInTabAtIndex:modelIndex];

  if (newContents) {
    newContents->DidBecomeSelected();
    newContents->view()->RestoreFocus();

    if (newContents->find_ui_active())
      browser_->GetFindBarController()->find_bar()->SetFocusAndSelection();
  }
}

// Remove all knowledge about this tab and it's associated
// controller and remove the view from the strip.
- (void)removeTab:(TabController*)controller {
  NSUInteger index = [tabArray_ indexOfObject:controller];

  // Release the tab contents controller so those views get destroyed. This
  // will remove all the tab content Cocoa views from the hierarchy. A
  // subsequent "select tab" notification will follow from the model. To
  // tell us what to swap in in its absence.
  [tabContentsArray_ removeObjectAtIndex:index];

  // Remove the view from the tab strip.
  NSView* tab = [controller view];
  [tab removeFromSuperview];

  // Clear the tab controller's target.
  // TODO(viettrungluu): [crbug.com/23829] Find a better way to handle the tab
  // controller's target.
  [controller setTarget:nil];

  if ([hoveredTab_ isEqual:tab])
    hoveredTab_ = nil;

  NSValue* identifier = [NSValue valueWithPointer:tab];
  [targetFrames_ removeObjectForKey:identifier];

  // Once we're totally done with the tab, delete its controller
  [tabArray_ removeObjectAtIndex:index];
}

// Called by the CAAnimation delegate when the tab completes the closing
// animation.
- (void)animationDidStopForController:(TabController*)controller
                             finished:(BOOL)finished {
  [closingControllers_ removeObject:controller];
  [self removeTab:controller];
}

// Save off which TabController is closing and tell its view's animator
// where to move the tab to. Registers a delegate to call back when the
// animation is complete in order to remove the tab from the model.
- (void)startClosingTabWithAnimation:(TabController*)closingTab {
  DCHECK([NSThread isMainThread]);
  // Save off the controller into the set of animating tabs. This alerts
  // the layout method to not do anything with it and allows us to correctly
  // calculate offsets when working with indices into the model.
  [closingControllers_ addObject:closingTab];

  // Mark the tab as closing. This prevents it from generating any drags or
  // selections while it's animating closed.
  [(TabView*)[closingTab view] setIsClosing:YES];

  // Register delegate (owned by the animation system).
  NSView* tabView = [closingTab view];
  CAAnimation* animation = [[tabView animationForKey:@"frameOrigin"] copy];
  [animation autorelease];
  scoped_nsobject<TabCloseAnimationDelegate> delegate(
    [[TabCloseAnimationDelegate alloc] initWithTabStrip:self
                                          tabController:closingTab]);
  [animation setDelegate:delegate.get()];  // Retains delegate.
  NSMutableDictionary* animationDictionary =
      [NSMutableDictionary dictionaryWithDictionary:[tabView animations]];
  [animationDictionary setObject:animation forKey:@"frameOrigin"];
  [tabView setAnimations:animationDictionary];

  // Periscope down! Animate the tab.
  NSRect newFrame = [tabView frame];
  newFrame = NSOffsetRect(newFrame, 0, -newFrame.size.height);
  ScopedNSAnimationContextGroup animationGroup(true);
  animationGroup.SetCurrentContextDuration(kAnimationDuration);
  [[tabView animator] setFrame:newFrame];
}

// Called when a notification is received from the model that the given tab
// has gone away. Start an animation then force a layout to put everything
// in motion.
- (void)tabDetachedWithContents:(TabContents*)contents
                        atIndex:(NSInteger)modelIndex {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  TabController* tab = [tabArray_ objectAtIndex:index];
  if (tabModel_->count() > 0) {
    [self startClosingTabWithAnimation:tab];
    [self layoutTabs];
  } else {
    [self removeTab:tab];
  }

  // Send a broadcast that the number of tabs have changed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabStripNumberOfTabsChanged
                    object:self];
}

// A helper routine for creating an NSImageView to hold the fav icon for
// |contents|.
- (NSImageView*)favIconImageViewForContents:(TabContents*)contents {
  NSRect iconFrame = NSMakeRect(0, 0, 16, 16);
  NSImageView* view = [[[NSImageView alloc] initWithFrame:iconFrame]
                          autorelease];

  NSImage* image = nil;

  NavigationEntry* navEntry = contents->controller().GetLastCommittedEntry();
  if (navEntry != NULL) {
    NavigationEntry::FaviconStatus favIcon = navEntry->favicon();
    const SkBitmap& bitmap = favIcon.bitmap();
    if (favIcon.is_valid() && !bitmap.isNull())
      image = gfx::SkBitmapToNSImage(bitmap);
  }

  // Either we don't have a valid favicon or there was some issue converting it
  // from an SkBitmap. Either way, just show the default.
  if (!image)
    image = defaultFavIcon_.get();

  [view setImage:image];
  return view;
}

// Updates the current loading state, replacing the icon view with a favicon,
// a throbber, the default icon, or nothing at all. |index| is a controller
// array index, not a model index.
- (void)updateFavIconForContents:(TabContents*)contents
                         atIndex:(NSInteger)index {
  if (!contents)
    return;

  static NSImage* throbberWaitingImage =
      [ResourceBundle::GetSharedInstance().GetNSImageNamed(IDR_THROBBER_WAITING)
        retain];
  static NSImage* throbberLoadingImage =
      [ResourceBundle::GetSharedInstance().GetNSImageNamed(IDR_THROBBER)
        retain];
  static NSImage* sadFaviconImage =
      [ResourceBundle::GetSharedInstance().GetNSImageNamed(IDR_SAD_FAVICON)
        retain];

  TabController* tabController = [tabArray_ objectAtIndex:index];

  bool oldHasIcon = [tabController iconView] != nil;
  bool newHasIcon = contents->ShouldDisplayFavIcon();

  TabLoadingState oldState = [tabController loadingState];
  TabLoadingState newState = kTabDone;
  NSImage* throbberImage = nil;
  if (contents->is_crashed()) {
    newState = kTabCrashed;
    newHasIcon = true;
  } else if (contents->waiting_for_response()) {
    newState = kTabWaiting;
    throbberImage = throbberWaitingImage;
  } else if (contents->is_loading()) {
    newState = kTabLoading;
    throbberImage = throbberLoadingImage;
  }

  if (oldState != newState)
    [tabController setLoadingState:newState];

  // While loading, this function is called repeatedly with the same state.
  // To avoid expensive unnecessary view manipulation, only make changes when
  // the state is actually changing.  When loading is complete (kTabDone),
  // every call to this function is significant.
  if (newState == kTabDone || oldState != newState ||
      oldHasIcon != newHasIcon) {
    NSView* iconView = nil;
    if (newHasIcon) {
      if (newState == kTabDone) {
        iconView = [self favIconImageViewForContents:contents];
      } else if (newState == kTabCrashed) {
        NSImage* oldImage = [[self favIconImageViewForContents:contents] image];
        NSRect frame = NSMakeRect(0, 0, 16, 16);
        iconView = [ThrobberView toastThrobberViewWithFrame:frame
                                                beforeImage:oldImage
                                                 afterImage:sadFaviconImage];
      } else {
        NSRect frame = NSMakeRect(0, 0, 16, 16);
        iconView = [ThrobberView filmstripThrobberViewWithFrame:frame
                                                          image:throbberImage];
      }
    }

    [tabController setIconView:iconView];
  }
}

// Called when a notification is received from the model that the given tab
// has been updated. |loading| will be YES when we only want to update the
// throbber state, not anything else about the (partially) loading tab.
- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)modelIndex
                   loadingOnly:(BOOL)loading {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  if (!loading)
    [self setTabTitle:[tabArray_ objectAtIndex:index] withContents:contents];

  [self updateFavIconForContents:contents atIndex:index];

  TabContentsController* updatedController =
      [tabContentsArray_ objectAtIndex:index];
  [updatedController tabDidChange:contents];
}

// Called when a tab is moved (usually by drag&drop). Keep our parallel arrays
// in sync with the tab strip model.
- (void)tabMovedWithContents:(TabContents*)contents
                    fromIndex:(NSInteger)modelFrom
                      toIndex:(NSInteger)modelTo {
  // Take closing tabs into account.
  NSInteger from = [self indexFromModelIndex:modelFrom];
  NSInteger to = [self indexFromModelIndex:modelTo];

  scoped_nsobject<TabContentsController> movedController(
      [[tabContentsArray_ objectAtIndex:from] retain]);
  [tabContentsArray_ removeObjectAtIndex:from];
  [tabContentsArray_ insertObject:movedController.get() atIndex:to];
  scoped_nsobject<TabView> movedView(
      [[tabArray_ objectAtIndex:from] retain]);
  [tabArray_ removeObjectAtIndex:from];
  [tabArray_ insertObject:movedView.get() atIndex:to];

  [self layoutTabs];
}

- (void)setFrameOfSelectedTab:(NSRect)frame {
  NSView* view = [self selectedTabView];
  NSValue* identifier = [NSValue valueWithPointer:view];
  [targetFrames_ setObject:[NSValue valueWithRect:frame]
                    forKey:identifier];
  [view setFrame:frame];
}

- (NSView*)selectedTabView {
  int selectedIndex = tabModel_->selected_index();
  // Take closing tabs into account. They can't ever be selected.
  selectedIndex = [self indexFromModelIndex:selectedIndex];
  return [self viewAtIndex:selectedIndex];
}

// Find the index based on the x coordinate of the placeholder. If there is
// no placeholder, this returns the end of the tab strip.
- (int)indexOfPlaceholder {
  double placeholderX = placeholderFrame_.origin.x;
  int index = 0;
  int location = 0;
  const int count = tabModel_->count();
  while (index < count) {
    NSView* curr = [self viewAtIndex:index];
    // The placeholder tab works by changing the frame of the tab being dragged
    // to be the bounds of the placeholder, so we need to skip it while we're
    // iterating, otherwise we'll end up off by one.  Note This only effects
    // dragging to the right, not to the left.
    if (curr == placeholderTab_) {
      index++;
      continue;
    }
    if (placeholderX <= NSMinX([curr frame]))
      break;
    index++;
    location++;
  }
  return location;
}

// Move the given tab at index |from| in this window to the location of the
// current placeholder.
- (void)moveTabFromIndex:(NSInteger)from {
  int toIndex = [self indexOfPlaceholder];
  tabModel_->MoveTabContentsAt(from, toIndex, true);
}

// Drop a given TabContents at the location of the current placeholder. If there
// is no placeholder, it will go at the end. Used when dragging from another
// window when we don't have access to the TabContents as part of our strip.
// |frame| is in the coordinate system of the tab strip view and represents
// where the user dropped the new tab so it can be animated into its correct
// location when the tab is added to the model.
- (void)dropTabContents:(TabContents*)contents withFrame:(NSRect)frame {
  int index = [self indexOfPlaceholder];

  // Mark that the new tab being created should start at |frame|. It will be
  // reset as soon as the tab has been positioned.
  droppedTabFrame_ = frame;

  // Insert it into this tab strip. We want it in the foreground and to not
  // inherit the current tab's group.
  tabModel_->InsertTabContentsAt(index, contents, true, false);
}

// Called when the tab strip view changes size. As we only registered for
// changes on our view, we know it's only for our view. Layout w/out
// animations since they are blocked by the resize nested runloop. We need
// the views to adjust immediately. Neither the tabs nor their z-order are
// changed, so we don't need to update the subviews.
- (void)tabViewFrameChanged:(NSNotification*)info {
  [self layoutTabsWithAnimation:NO regenerateSubviews:NO];
}

// Called when the tracking areas for any given tab are updated. This allows
// the individual tabs to update their hover states correctly.
// Only generates the event if the cursor is in the tab strip.
- (void)tabUpdateTracking:(NSNotification*)notification {
  DCHECK([[notification object] isKindOfClass:[TabView class]]);
  DCHECK(mouseInside_);
  NSWindow* window = [tabView_ window];
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  if (NSPointInRect(location, [tabView_ frame])) {
    NSEvent* mouseEvent = [NSEvent mouseEventWithType:NSMouseMoved
                                             location:location
                                        modifierFlags:0
                                            timestamp:0
                                         windowNumber:[window windowNumber]
                                              context:nil
                                          eventNumber:0
                                           clickCount:0
                                             pressure:0];
    [self mouseMoved:mouseEvent];
  }
}

- (BOOL)inRapidClosureMode {
  return availableResizeWidth_ != kUseFullAvailableWidth;
}

// Disable tab dragging when there are any pending animations.
- (BOOL)tabDraggingAllowed {
  return [closingControllers_ count] == 0;
}

- (void)mouseMoved:(NSEvent*)event {
  // Use hit test to figure out what view we are hovering over.
  TabView* targetView = (TabView*)[tabView_ hitTest:[event locationInWindow]];
  if (![targetView isKindOfClass:[TabView class]]) {
    if ([[targetView superview] isKindOfClass:[TabView class]]) {
      targetView = (TabView*)[targetView superview];
    } else {
      targetView = nil;
    }
  }

  if (hoveredTab_ != targetView) {
    [hoveredTab_ mouseExited:nil];  // We don't pass event because moved events
    [targetView mouseEntered:nil];  // don't have valid tracking areas
    hoveredTab_ = targetView;
  } else {
    [hoveredTab_ mouseMoved:event];
  }
}

- (void)mouseEntered:(NSEvent*)event {
  NSTrackingArea* area = [event trackingArea];
  if ([area isEqual:trackingArea_]) {
    mouseInside_ = YES;
    [self setTabTrackingAreasEnabled:YES];
    [self mouseMoved:event];
  } else if ([area isEqual:newTabTrackingArea_]) {
    [newTabButton_ setImage:nsimage_cache::ImageNamed(@"newtab_h.pdf")];
  }
}

// Called when the tracking area is in effect which means we're tracking to
// see if the user leaves the tab strip with their mouse. When they do,
// reset layout to use all available width.
- (void)mouseExited:(NSEvent*)event {
  NSTrackingArea* area = [event trackingArea];
  if ([area isEqual:trackingArea_]) {
    mouseInside_ = NO;
    [self setTabTrackingAreasEnabled:NO];
    availableResizeWidth_ = kUseFullAvailableWidth;
    [hoveredTab_ mouseExited:event];
    hoveredTab_ = nil;
    [self layoutTabs];
  } else if ([area isEqual:newTabTrackingArea_]) {
    [newTabButton_ setImage:nsimage_cache::ImageNamed(@"newtab.pdf")];
  }
}

// Enable/Disable the tracking areas for the tabs. They are only enabled
// when the mouse is in the tabstrip.
- (void)setTabTrackingAreasEnabled:(BOOL)enabled {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  for (TabController* controller in tabArray_.get()) {
    TabView* tabView = [controller tabView];
    if (enabled) {
      // Set self up to observe tabs so hover states will be correct.
      [defaultCenter addObserver:self
                        selector:@selector(tabUpdateTracking:)
                            name:NSViewDidUpdateTrackingAreasNotification
                          object:tabView];
    } else {
      [defaultCenter removeObserver:self
                               name:NSViewDidUpdateTrackingAreasNotification
                             object:tabView];
    }
    [tabView setTrackingEnabled:enabled];
  }
}

// Adds the given subview to (the end of) the list of permanent subviews
// (specified from bottom up). These subviews will always be below the
// transitory subviews (tabs). |-regenerateSubviewList| must be called to
// effectuate the addition.
- (void)addSubviewToPermanentList:(NSView*)aView {
  [permanentSubviews_ addObject:aView];
}

// Update the subviews, keeping the permanent ones (or, more correctly, putting
// in the ones listed in permanentSubviews_), and putting in the current tabs in
// the correct z-order. Any current subviews which is neither in the permanent
// list nor a (current) tab will be removed. So if you add such a subview, you
// should call |-addSubviewToPermanentList:| (or better yet, call that and then
// |-regenerateSubviewList| to actually add it).
- (void)regenerateSubviewList {
  // Remove self as an observer from all the old tabs before a new set of
  // potentially different tabs is put in place.
  [self setTabTrackingAreasEnabled:NO];

  // Subviews to put in (in bottom-to-top order), beginning with the permanent
  // ones.
  NSMutableArray* subviews = [NSMutableArray arrayWithArray:permanentSubviews_];

  NSView* selectedTabView = nil;
  // Go through tabs in reverse order, since |subviews| is bottom-to-top.
  for (TabController* tab in [tabArray_ reverseObjectEnumerator]) {
    NSView* tabView = [tab view];
    if ([tab selected]) {
      DCHECK(!selectedTabView);
      selectedTabView = tabView;
    } else {
      [subviews addObject:tabView];
    }
  }
  if (selectedTabView) {
    [subviews addObject:selectedTabView];
  }
  [tabView_ setSubviews:subviews];
  [self setTabTrackingAreasEnabled:mouseInside_];
}

- (GTMWindowSheetController*)sheetController {
  if (!sheetController_.get())
    sheetController_.reset([[GTMWindowSheetController alloc]
        initWithWindow:[switchView_ window] delegate:self]);
  return sheetController_.get();
}

- (void)gtm_systemRequestsVisibilityForView:(NSView*)view {
  // This implementation is required by GTMWindowSheetController.

  // Raise window...
  [[switchView_ window] makeKeyAndOrderFront:self];

  // ...and raise a tab with a sheet.
  NSInteger index = [self modelIndexForContentsView:view];
  DCHECK(index >= 0);
  if (index >= 0)
    tabModel_->SelectTabContentsAt(index, false /* not a user gesture */);
}

- (BOOL)attachConstrainedWindow:(ConstrainedWindowMac*)window {
  // TODO(thakis, avi): Figure out how to make this work when tabs are dragged
  // out or if fullscreen mode is toggled.

  // View hierarchy of the contents view:
  // NSView  -- switchView, same for all tabs
  // +-  NSView  -- TabContentsController's view
  //     +- NSBox
  //        +- TabContentsViewCocoa
  // We use the TabContentsController's view in |swapInTabAtIndex|, so we have
  // to pass it to the sheet controller here.
  NSView* tabContentsView =
      [[window->owner()->GetNativeView() superview] superview];

  // TODO(avi, thakis): GTMWindowSheetController has no api to move tabsheets
  // between windows. Until then, we have to prevent having to move a tabsheet
  // between windows, e.g. no tearing off of tabs.
  NSInteger modelIndex = [self modelIndexForContentsView:tabContentsView];
  NSInteger index = [self indexFromModelIndex:modelIndex];
  BrowserWindowController* controller =
      (BrowserWindowController*)[[switchView_ window] windowController];
  DCHECK(controller != nil);
  DCHECK(index >= 0);
  if (index >= 0) {
    NSView* tab = [self viewAtIndex:index];
    [controller setTab:tab isDraggable:NO];

    std::deque<ConstrainedWindowMac*>& windows = constrainedWindows_[tab];
    std::deque<ConstrainedWindowMac*>::iterator it =
        find(windows.begin(), windows.end(), window);
    if (it == windows.end())
      constrainedWindows_[tab].push_back(window);

    if (constrainedWindows_[tab].size() == 1) {
      [controller setTab:tab isDraggable:NO];
      window->SetVisible();
      window->delegate()->RunSheet([self sheetController], tabContentsView);
      return YES;
    }
  }
  return NO;
}

- (void)removeConstrainedWindow:(ConstrainedWindowMac*)window {
  NSView* tabContentsView =
      [[window->owner()->GetNativeView() superview] superview];

  // TODO(avi, thakis): GTMWindowSheetController has no api to move tabsheets
  // between windows. Until then, we have to prevent having to move a tabsheet
  // between windows, e.g. no tearing off of tabs.
  NSInteger modelIndex = [self modelIndexForContentsView:tabContentsView];
  NSInteger index = [self indexFromModelIndex:modelIndex];
  BrowserWindowController* controller =
      (BrowserWindowController*)[[switchView_ window] windowController];
  DCHECK(index >= 0);
  if (index >= 0) {
    NSView* tab = [self viewAtIndex:index];

    std::deque<ConstrainedWindowMac*>& windows = constrainedWindows_[tab];
    std::deque<ConstrainedWindowMac*>::iterator it =
        find(windows.begin(), windows.end(), window);
    DCHECK(it != windows.end());

    bool removedVisibleSheet = it == windows.begin();

    if (it != windows.end())
      windows.erase(it);

    if (windows.size() == 0) {
      [controller setTab:tab isDraggable:YES];
      constrainedWindows_.erase(tab);
    } else if (removedVisibleSheet && tab == [self selectedTabView]) {
      // Show next sheet
      NSWindowController* controller = [[tab window] windowController];
      DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
      windows.front()->Realize(
          static_cast<BrowserWindowController*>(controller));
    }
  }
}

@end
