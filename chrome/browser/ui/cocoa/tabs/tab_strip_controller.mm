// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"

#import <QuartzCore/QuartzCore.h>

#include <cmath>
#include <limits>
#include <string>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "chrome/browser/ui/cocoa/drag_util.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/new_tab_button.h"
#import "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/media_indicator_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_drag_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_model_observer_bridge.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/url_fixer/url_fixer.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/scoped_ns_disable_screen_updates.h"

using base::UserMetricsAction;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// A value to indicate tab layout should use the full available width of the
// view.
const CGFloat kUseFullAvailableWidth = -1.0;

// The amount by which tabs overlap.
// Needs to be <= the x position of the favicon within a tab. Else, every time
// the throbber is painted, the throbber's invalidation will also invalidate
// parts of the tab to the left, and two tabs's backgrounds need to be painted
// on each throbber frame instead of one.
const CGFloat kTabOverlap = 19.0;

// The amount by which mini tabs are separated from normal tabs.
const CGFloat kLastMiniTabSpacing = 2.0;

// The amount by which the new tab button is offset (from the tabs).
const CGFloat kNewTabButtonOffset = 8.0;

// Time (in seconds) in which tabs animate to their final position.
const NSTimeInterval kAnimationDuration = 0.125;

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
      [[NSAnimationContext currentContext] gtm_setDuration:duration
                                                 eventMask:NSLeftMouseUpMask];
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

// Creates an NSImage with size |size| and bitmap image representations for both
// 1x and 2x scale factors. |drawingHandler| is called once for every scale
// factor.  This is similar to -[NSImage imageWithSize:flipped:drawingHandler:],
// but this function always evaluates drawingHandler eagerly, and it works on
// 10.6 and 10.7.
NSImage* CreateImageWithSize(NSSize size,
                             void (^drawingHandler)(NSSize)) {
  base::scoped_nsobject<NSImage> result([[NSImage alloc] initWithSize:size]);
  [NSGraphicsContext saveGraphicsState];
  for (ui::ScaleFactor scale_factor : ui::GetSupportedScaleFactors()) {
    float scale = GetScaleForScaleFactor(scale_factor);
    NSBitmapImageRep *bmpImageRep = [[[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:size.width * scale
                      pixelsHigh:size.height * scale
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:0
                    bitsPerPixel:0] autorelease];
    [bmpImageRep setSize:size];
    [NSGraphicsContext setCurrentContext:
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bmpImageRep]];
    drawingHandler(size);
    [result addRepresentation:bmpImageRep];
  }
  [NSGraphicsContext restoreGraphicsState];

  return result.release();
}

// Takes a normal bitmap and a mask image and returns an image the size of the
// mask that has pixels from |image| but alpha information from |mask|.
NSImage* ApplyMask(NSImage* image, NSImage* mask) {
  return [CreateImageWithSize([mask size], ^(NSSize size) {
      // Skip a few pixels from the top of the tab background gradient, because
      // the new tab button is not drawn at the very top of the browser window.
      const int kYOffset = 10;
      CGFloat width = size.width;
      CGFloat height = size.height;

      // In some themes, the tab background image is narrower than the
      // new tab button, so tile the background image.
      CGFloat x = 0;
      // The floor() is to make sure images with odd widths don't draw to the
      // same pixel twice on retina displays. (Using NSDrawThreePartImage()
      // caused a startup perf regression, so that cannot be used.)
      CGFloat tileWidth = floor(std::min(width, [image size].width));
      while (x < width) {
        [image drawAtPoint:NSMakePoint(x, 0)
                  fromRect:NSMakeRect(0,
                                      [image size].height - height - kYOffset,
                                      tileWidth,
                                      height)
                 operation:NSCompositeCopy
                  fraction:1.0];
        x += tileWidth;
      }

      [mask drawAtPoint:NSZeroPoint
               fromRect:NSMakeRect(0, 0, width, height)
              operation:NSCompositeDestinationIn
               fraction:1.0];
  }) autorelease];
}

// Paints |overlay| on top of |ground|.
NSImage* Overlay(NSImage* ground, NSImage* overlay, CGFloat alpha) {
  DCHECK_EQ([ground size].width, [overlay size].width);
  DCHECK_EQ([ground size].height, [overlay size].height);

  return [CreateImageWithSize([ground size], ^(NSSize size) {
      CGFloat width = size.width;
      CGFloat height = size.height;
      [ground drawAtPoint:NSZeroPoint
                 fromRect:NSMakeRect(0, 0, width, height)
                operation:NSCompositeCopy
                 fraction:1.0];
      [overlay drawAtPoint:NSZeroPoint
                  fromRect:NSMakeRect(0, 0, width, height)
                 operation:NSCompositeSourceOver
                  fraction:alpha];
  }) autorelease];
}

}  // namespace

@interface TabStripController (Private)
- (void)addSubviewToPermanentList:(NSView*)aView;
- (void)regenerateSubviewList;
- (NSInteger)indexForContentsView:(NSView*)view;
- (NSImage*)iconImageForContents:(content::WebContents*)contents;
- (void)updateIconsForContents:(content::WebContents*)contents
                       atIndex:(NSInteger)modelIndex;
- (void)layoutTabsWithAnimation:(BOOL)animate
             regenerateSubviews:(BOOL)doUpdate;
- (void)animationDidStop:(CAAnimation*)animation
           forController:(TabController*)controller
                finished:(BOOL)finished;
- (NSInteger)indexFromModelIndex:(NSInteger)index;
- (void)clickNewTabButton:(id)sender;
- (NSInteger)numberOfOpenTabs;
- (NSInteger)numberOfOpenMiniTabs;
- (NSInteger)numberOfOpenNonMiniTabs;
- (void)mouseMoved:(NSEvent*)event;
- (void)setTabTrackingAreasEnabled:(BOOL)enabled;
- (void)droppingURLsAt:(NSPoint)point
            givesIndex:(NSInteger*)index
           disposition:(WindowOpenDisposition*)disposition;
- (void)setNewTabButtonHoverState:(BOOL)showHover;
- (void)themeDidChangeNotification:(NSNotification*)notification;
- (void)setNewTabImages;
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

// Runs a nested runloop to do window move tracking. Overriding
// -mouseDownCanMoveWindow with a dynamic result instead doesn't work:
// http://www.cocoabuilder.com/archive/cocoa/219261-conditional-mousedowncanmovewindow-for-nsview.html
// http://www.cocoabuilder.com/archive/cocoa/92973-brushed-metal-window-dragging.html
- (void)trackClickForWindowMove:(NSEvent*)event;
@end

@implementation TabStripControllerDragBlockingView
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)drawRect:(NSRect)rect {
}

- (id)initWithFrame:(NSRect)frameRect
         controller:(TabStripController*)controller {
  if ((self = [super initWithFrame:frameRect])) {
    controller_ = controller;
  }
  return self;
}

// In "rapid tab closure" mode (i.e., the user is clicking close tab buttons in
// rapid succession), the animations confuse Cocoa's hit testing (which appears
// to use cached results, among other tricks), so this view can somehow end up
// getting a mouse down event. Thus we do an explicit hit test during rapid tab
// closure, and if we find that we got a mouse down we shouldn't have, we send
// it off to the appropriate view.
- (void)mouseDown:(NSEvent*)event {
  NSView* superview = [self superview];
  NSPoint hitLocation =
      [[superview superview] convertPoint:[event locationInWindow]
                                 fromView:nil];
  NSView* hitView = [superview hitTest:hitLocation];

  if ([controller_ inRapidClosureMode]) {
    if (hitView != self) {
      [hitView mouseDown:event];
      return;
    }
  }

  if (hitView == self) {
    BrowserWindowController* windowController =
        [BrowserWindowController browserWindowControllerForView:self];
    if (![windowController isFullscreen]) {
      [self trackClickForWindowMove:event];
      return;
    }
  }
  [super mouseDown:event];
}

- (void)trackClickForWindowMove:(NSEvent*)event {
  NSWindow* window = [self window];
  NSPoint frameOrigin = [window frame].origin;
  NSPoint lastEventLoc = [window convertBaseToScreen:[event locationInWindow]];
  while ((event = [NSApp nextEventMatchingMask:
      NSLeftMouseDownMask|NSLeftMouseDraggedMask|NSLeftMouseUpMask
                                    untilDate:[NSDate distantFuture]
                                       inMode:NSEventTrackingRunLoopMode
                                      dequeue:YES]) &&
      [event type] != NSLeftMouseUp) {
    base::mac::ScopedNSAutoreleasePool pool;

    NSPoint now = [window convertBaseToScreen:[event locationInWindow]];
    frameOrigin.x += now.x - lastEventLoc.x;
    frameOrigin.y += now.y - lastEventLoc.y;
    [window setFrameOrigin:frameOrigin];
    lastEventLoc = now;
  }
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
  if ((self = [super init])) {
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
  [strip_ animationDidStop:animation
             forController:controller_
                  finished:finished];
}

@end

#pragma mark -

// In general, there is a one-to-one correspondence between TabControllers,
// TabViews, TabContentsControllers, and the WebContents in the
// TabStripModel. In the steady-state, the indices line up so an index coming
// from the model is directly mapped to the same index in the parallel arrays
// holding our views and controllers. This is also true when new tabs are
// created (even though there is a small period of animation) because the tab is
// present in the model while the TabView is animating into place. As a result,
// nothing special need be done to handle "new tab" animation.
//
// This all goes out the window with the "close tab" animation. The animation
// kicks off in |-tabDetachedWithContents:atIndex:| with the notification that
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

@synthesize leftIndentForControls = leftIndentForControls_;
@synthesize rightIndentForControls = rightIndentForControls_;

- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
           browser:(Browser*)browser
          delegate:(id<TabStripControllerDelegate>)delegate {
  DCHECK(view && switchView && browser && delegate);
  if ((self = [super init])) {
    tabStripView_.reset([view retain]);
    [tabStripView_ setController:self];
    switchView_ = switchView;
    browser_ = browser;
    tabStripModel_ = browser_->tab_strip_model();
    hoverTabSelector_.reset(new HoverTabSelector(tabStripModel_));
    delegate_ = delegate;
    bridge_.reset(new TabStripModelObserverBridge(tabStripModel_, self));
    dragController_.reset(
        [[TabStripDragController alloc] initWithTabStripController:self]);
    tabContentsArray_.reset([[NSMutableArray alloc] init]);
    tabArray_.reset([[NSMutableArray alloc] init]);
    NSWindow* browserWindow = [view window];

    // Important note: any non-tab subviews not added to |permanentSubviews_|
    // (see |-addSubviewToPermanentList:|) will be wiped out.
    permanentSubviews_.reset([[NSMutableArray alloc] init]);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    defaultFavicon_.reset(
        rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).CopyNSImage());

    [self setLeftIndentForControls:[[self class] defaultLeftIndentForControls]];
    [self setRightIndentForControls:0];

    // Add this invisible view first so that it is ordered below other views.
    dragBlockingView_.reset(
        [[TabStripControllerDragBlockingView alloc] initWithFrame:NSZeroRect
                                                       controller:self]);
    [self addSubviewToPermanentList:dragBlockingView_];

    newTabButton_ = [view getNewTabButton];
    [newTabButton_ setWantsLayer:YES];
    [self addSubviewToPermanentList:newTabButton_];
    [newTabButton_ setTarget:self];
    [newTabButton_ setAction:@selector(clickNewTabButton:)];

    [self setNewTabImages];
    newTabButtonShowingHoverImage_ = NO;
    newTabTrackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:[newTabButton_ bounds]
                                     options:(NSTrackingMouseEnteredAndExited |
                                              NSTrackingActiveAlways)
                                       owner:self
                                    userInfo:nil]);
    if (browserWindow)  // Nil for Browsers without a tab strip (e.g. popups).
      [newTabTrackingArea_ clearOwnerWhenWindowWillClose:browserWindow];
    [newTabButton_ addTrackingArea:newTabTrackingArea_.get()];
    targetFrames_.reset([[NSMutableDictionary alloc] init]);

    newTabTargetFrame_ = NSZeroRect;
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
             object:tabStripView_];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(themeDidChangeNotification:)
               name:kBrowserThemeDidChangeNotification
             object:nil];

    trackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:NSZeroRect  // Ignored by NSTrackingInVisibleRect
             options:NSTrackingMouseEnteredAndExited |
                     NSTrackingMouseMoved |
                     NSTrackingActiveAlways |
                     NSTrackingInVisibleRect
               owner:self
            userInfo:nil]);
    if (browserWindow)  // Nil for Browsers without a tab strip (e.g. popups).
      [trackingArea_ clearOwnerWhenWindowWillClose:browserWindow];
    [tabStripView_ addTrackingArea:trackingArea_.get()];

    // Check to see if the mouse is currently in our bounds so we can
    // enable the tracking areas.  Otherwise we won't get hover states
    // or tab gradients if we load the window up under the mouse.
    NSPoint mouseLoc = [[view window] mouseLocationOutsideOfEventStream];
    mouseLoc = [view convertPoint:mouseLoc fromView:nil];
    if (NSPointInRect(mouseLoc, [view bounds])) {
      [self setTabTrackingAreasEnabled:YES];
      mouseInside_ = YES;
    }

    // Set accessibility descriptions. http://openradar.appspot.com/7496255
    NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_NEWTAB);
    [[newTabButton_ cell]
        accessibilitySetOverrideValue:description
                         forAttribute:NSAccessibilityDescriptionAttribute];

    // Controller may have been (re-)created by switching layout modes, which
    // means the tab model is already fully formed with tabs. Need to walk the
    // list and create the UI for each.
    const int existingTabCount = tabStripModel_->count();
    const content::WebContents* selection =
        tabStripModel_->GetActiveWebContents();
    for (int i = 0; i < existingTabCount; ++i) {
      content::WebContents* currentContents =
          tabStripModel_->GetWebContentsAt(i);
      [self insertTabWithContents:currentContents
                          atIndex:i
                     inForeground:NO];
      if (selection == currentContents) {
        // Must manually force a selection since the model won't send
        // selection messages in this scenario.
        [self
            activateTabWithContents:currentContents
                   previousContents:NULL
                            atIndex:i
                             reason:TabStripModelObserver::CHANGE_REASON_NONE];
      }
    }
    // Don't lay out the tabs until after the controller has been fully
    // constructed.
    if (existingTabCount) {
      [self performSelectorOnMainThread:@selector(layoutTabs)
                             withObject:nil
                          waitUntilDone:NO];
    }
  }
  return self;
}

- (void)dealloc {
  [tabStripView_ setController:nil];

  if (trackingArea_.get())
    [tabStripView_ removeTrackingArea:trackingArea_.get()];

  [newTabButton_ removeTrackingArea:newTabTrackingArea_.get()];
  // Invalidate all closing animations so they don't call back to us after
  // we're gone.
  for (TabController* controller in closingControllers_.get()) {
    NSView* view = [controller view];
    [[[view animationForKey:@"frameOrigin"] delegate] invalidate];
  }
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [tabStripView_ removeAllToolTips];
  [super dealloc];
}

+ (CGFloat)defaultTabHeight {
  return 26.0;
}

+ (CGFloat)defaultLeftIndentForControls {
  // Default indentation leaves enough room so tabs don't overlap with the
  // window controls.
  return 70.0;
}

// Finds the TabContentsController associated with the given index into the tab
// model and swaps out the sole child of the contentArea to display its
// contents.
- (void)swapInTabAtIndex:(NSInteger)modelIndex {
  DCHECK(modelIndex >= 0 && modelIndex < tabStripModel_->count());
  NSInteger index = [self indexFromModelIndex:modelIndex];
  TabContentsController* controller = [tabContentsArray_ objectAtIndex:index];

  // Make sure we do not draw any transient arrangements of views.
  gfx::ScopedNSDisableScreenUpdates ns_disabler;
  // Make sure that any layers that move are not animated to their new
  // positions.
  ScopedCAActionDisabler ca_disabler;

  // Resize the new view to fit the window. Calling |view| may lazily
  // instantiate the TabContentsController from the nib. Until we call
  // |-ensureContentsVisible|, the controller doesn't install the RWHVMac into
  // the view hierarchy. This is in order to avoid sending the renderer a
  // spurious default size loaded from the nib during the call to |-view|.
  NSView* newView = [controller view];

  // Turns content autoresizing off, so removing and inserting views won't
  // trigger unnecessary content relayout.
  [controller ensureContentsSizeDoesNotChange];

  // Remove the old view from the view hierarchy. We know there's only one
  // child of |switchView_| because we're the one who put it there. There
  // may not be any children in the case of a tab that's been closed, in
  // which case there's no swapping going on.
  NSArray* subviews = [switchView_ subviews];
  if ([subviews count]) {
    NSView* oldView = [subviews objectAtIndex:0];
    // Set newView frame to the oldVew frame to prevent NSSplitView hosting
    // sidebar and tab content from resizing sidebar's content view.
    // ensureContentsVisible (see below) sets content size and autoresizing
    // properties.
    [newView setFrame:[oldView frame]];
    [switchView_ replaceSubview:oldView with:newView];
  } else {
    [newView setFrame:[switchView_ bounds]];
    [switchView_ addSubview:newView];
  }

  // New content is in place, delegate should adjust itself accordingly.
  [delegate_ onActivateTabWithContents:[controller webContents]];

  // It also restores content autoresizing properties.
  [controller ensureContentsVisible];

  NSWindow* parentWindow = [switchView_ window];
  ConstrainedWindowSheetController* sheetController =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parentWindow];
  [sheetController parentViewDidBecomeActive:newView];
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

// (Private) Handles a click on the new tab button.
- (void)clickNewTabButton:(id)sender {
  content::RecordAction(UserMetricsAction("NewTab_Button"));
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_BUTTON,
                            TabStripModel::NEW_TAB_ENUM_COUNT);
  tabStripModel_->delegate()->AddTabAt(GURL(), -1, true);
}

// (Private) Returns the number of open tabs in the tab strip. This is the
// number of TabControllers we know about (as there's a 1-to-1 mapping from
// these controllers to a tab) less the number of closing tabs.
- (NSInteger)numberOfOpenTabs {
  return static_cast<NSInteger>(tabStripModel_->count());
}

// (Private) Returns the number of open, mini-tabs.
- (NSInteger)numberOfOpenMiniTabs {
  // Ask the model for the number of mini tabs. Note that tabs which are in
  // the process of closing (i.e., whose controllers are in
  // |closingControllers_|) have already been removed from the model.
  return tabStripModel_->IndexOfFirstNonMiniTab();
}

// (Private) Returns the number of open, non-mini tabs.
- (NSInteger)numberOfOpenNonMiniTabs {
  NSInteger number = [self numberOfOpenTabs] - [self numberOfOpenMiniTabs];
  DCHECK_GE(number, 0);
  return number;
}

// Given an index into the tab model, returns the index into the tab controller
// or tab contents controller array accounting for tabs that are currently
// closing. For example, if there are two tabs in the process of closing before
// |index|, this returns |index| + 2. If there are no closing tabs, this will
// return |index|.
- (NSInteger)indexFromModelIndex:(NSInteger)index {
  DCHECK_GE(index, 0);
  if (index < 0)
    return index;

  NSInteger i = 0;
  for (TabController* controller in tabArray_.get()) {
    if ([closingControllers_ containsObject:controller]) {
      DCHECK([[controller tabView] isClosing]);
      ++index;
    }
    if (i == index)  // No need to check anything after, it has no effect.
      break;
    ++i;
  }
  return index;
}

// Given an index into |tabArray_|, return the corresponding index into
// |tabStripModel_| or NSNotFound if the specified tab does not exist in
// the model (if it's closing, for example).
- (NSInteger)modelIndexFromIndex:(NSInteger)index {
  NSInteger modelIndex = 0;
  NSInteger arrayIndex = 0;
  for (TabController* controller in tabArray_.get()) {
    if (![closingControllers_ containsObject:controller]) {
      if (arrayIndex == index)
        return modelIndex;
      ++modelIndex;
    } else if (arrayIndex == index) {
      // Tab is closing - no model index.
      return NSNotFound;
    }
    ++arrayIndex;
  }
  return NSNotFound;
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

- (NSArray*)selectedViews {
  NSMutableArray* views = [NSMutableArray arrayWithCapacity:[tabArray_ count]];
  for (TabController* tab in tabArray_.get()) {
    if ([tab selected])
      [views addObject:[tab tabView]];
  }
  return views;
}

// Returns the view at the given index, using the array of TabControllers to
// get the associated view. Returns nil if out of range.
- (NSView*)viewAtIndex:(NSUInteger)index {
  if (index >= [tabArray_ count])
    return NULL;
  return [[tabArray_ objectAtIndex:index] view];
}

- (NSUInteger)viewsCount {
  return [tabArray_ count];
}

// Called when the user clicks a tab. Tell the model the selection has changed,
// which feeds back into us via a notification.
- (void)selectTab:(id)sender {
  DCHECK([sender isKindOfClass:[NSView class]]);
  int index = [self modelIndexForTabView:sender];
  NSUInteger modifiers = [[NSApp currentEvent] modifierFlags];
  if (tabStripModel_->ContainsIndex(index)) {
    if (modifiers & NSCommandKeyMask && modifiers & NSShiftKeyMask) {
      tabStripModel_->AddSelectionFromAnchorTo(index);
    } else if (modifiers & NSShiftKeyMask) {
      tabStripModel_->ExtendSelectionTo(index);
    } else if (modifiers & NSCommandKeyMask) {
      tabStripModel_->ToggleSelectionAt(index);
    } else {
      tabStripModel_->ActivateTabAt(index, true);
    }
  }
}

// Called when the user closes a tab. Asks the model to close the tab. |sender|
// is the TabView that is potentially going away.
- (void)closeTab:(id)sender {
  DCHECK([sender isKindOfClass:[TabView class]]);

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  if ([hoveredTab_ isEqual:sender]) {
    hoveredTab_ = nil;
  }

  NSInteger index = [self modelIndexForTabView:sender];
  if (!tabStripModel_->ContainsIndex(index))
    return;

  content::RecordAction(UserMetricsAction("CloseTab_Mouse"));
  const NSInteger numberOfOpenTabs = [self numberOfOpenTabs];
  if (numberOfOpenTabs > 1) {
    bool isClosingLastTab = index == numberOfOpenTabs - 1;
    if (!isClosingLastTab) {
      // Limit the width available for laying out tabs so that tabs are not
      // resized until a later time (when the mouse leaves the tab strip).
      // However, if the tab being closed is a pinned tab, break out of
      // rapid-closure mode since the mouse is almost guaranteed not to be over
      // the closebox of the adjacent tab (due to the difference in widths).
      // TODO(pinkerton): re-visit when handling tab overflow.
      // http://crbug.com/188
      if (tabStripModel_->IsTabPinned(index)) {
        availableResizeWidth_ = kUseFullAvailableWidth;
      } else {
        NSView* penultimateTab = [self viewAtIndex:numberOfOpenTabs - 2];
        availableResizeWidth_ = NSMaxX([penultimateTab frame]);
      }
    } else {
      // If the rightmost tab is closed, change the available width so that
      // another tab's close button lands below the cursor (assuming the tabs
      // are currently below their maximum width and can grow).
      NSView* lastTab = [self viewAtIndex:numberOfOpenTabs - 1];
      availableResizeWidth_ = NSMaxX([lastTab frame]);
    }
    tabStripModel_->CloseWebContentsAt(
        index,
        TabStripModel::CLOSE_USER_GESTURE |
        TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
  } else {
    // Use the standard window close if this is the last tab
    // this prevents the tab from being removed from the model until after
    // the window dissapears
    [[tabStripView_ window] performClose:nil];
  }
}

// Dispatch context menu commands for the given tab controller.
- (void)commandDispatch:(TabStripModel::ContextMenuCommand)command
          forController:(TabController*)controller {
  int index = [self modelIndexForTabView:[controller view]];
  if (tabStripModel_->ContainsIndex(index))
    tabStripModel_->ExecuteContextMenuCommand(index, command);
}

// Returns YES if the specificed command should be enabled for the given
// controller.
- (BOOL)isCommandEnabled:(TabStripModel::ContextMenuCommand)command
           forController:(TabController*)controller {
  int index = [self modelIndexForTabView:[controller view]];
  if (!tabStripModel_->ContainsIndex(index))
    return NO;
  return tabStripModel_->IsContextMenuCommandEnabled(index, command) ? YES : NO;
}

// Returns a context menu model for a given controller. Caller owns the result.
- (ui::SimpleMenuModel*)contextMenuModelForController:(TabController*)controller
    menuDelegate:(ui::SimpleMenuModel::Delegate*)delegate {
  int index = [self modelIndexForTabView:[controller view]];
  return new TabMenuModel(delegate, tabStripModel_, index);
}

// Returns a weak reference to the controller that manages dragging of tabs.
- (id<TabDraggingEventTarget>)dragController {
  return dragController_.get();
}

- (void)insertPlaceholderForTab:(TabView*)tab frame:(NSRect)frame {
  placeholderTab_ = tab;
  placeholderFrame_ = frame;
  [self layoutTabsWithAnimation:initialLayoutComplete_ regenerateSubviews:NO];
}

- (BOOL)isDragSessionActive {
  return placeholderTab_ != nil;
}

- (BOOL)isTabFullyVisible:(TabView*)tab {
  NSRect frame = [tab frame];
  return NSMinX(frame) >= [self leftIndentForControls] &&
      NSMaxX(frame) <= (NSMaxX([tabStripView_ frame]) -
                        [self rightIndentForControls]);
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
// TODO(pinkerton): Note this doesn't do too well when the number of min-sized
// tabs would cause an overflow. http://crbug.com/188
- (void)layoutTabsWithAnimation:(BOOL)animate
             regenerateSubviews:(BOOL)doUpdate {
  DCHECK([NSThread isMainThread]);
  if (![tabArray_ count])
    return;

  const CGFloat kMaxTabWidth = [TabController maxTabWidth];
  const CGFloat kMinTabWidth = [TabController minTabWidth];
  const CGFloat kMinSelectedTabWidth = [TabController minSelectedTabWidth];
  const CGFloat kMiniTabWidth = [TabController miniTabWidth];
  const CGFloat kAppTabWidth = [TabController appTabWidth];

  NSRect enclosingRect = NSZeroRect;
  ScopedNSAnimationContextGroup mainAnimationGroup(animate);
  mainAnimationGroup.SetCurrentContextDuration(kAnimationDuration);

  // Update the current subviews and their z-order if requested.
  if (doUpdate)
    [self regenerateSubviewList];

  // Compute the base width of tabs given how much room we're allowed. Note that
  // mini-tabs have a fixed width. We may not be able to use the entire width
  // if the user is quickly closing tabs. This may be negative, but that's okay
  // (taken care of by |MAX()| when calculating tab sizes).
  CGFloat availableSpace = 0;
  if ([self inRapidClosureMode]) {
    availableSpace = availableResizeWidth_;
  } else {
    availableSpace = NSWidth([tabStripView_ frame]);

    // Account for the width of the new tab button.
    availableSpace -= NSWidth([newTabButton_ frame]) + kNewTabButtonOffset;

    // Account for the right-side controls if not in rapid closure mode.
    // (In rapid closure mode, the available width is set based on the
    // position of the rightmost tab, not based on the width of the tab strip,
    // so the right controls have already been accounted for.)
    availableSpace -= [self rightIndentForControls];
  }

  // Need to leave room for the left-side controls even in rapid closure mode.
  availableSpace -= [self leftIndentForControls];

  // If there are any mini tabs, account for the extra spacing between the last
  // mini tab and the first regular tab.
  if ([self numberOfOpenMiniTabs])
    availableSpace -= kLastMiniTabSpacing;

  // This may be negative, but that's okay (taken care of by |MAX()| when
  // calculating tab sizes). "mini" tabs in horizontal mode just get a special
  // section, they don't change size.
  CGFloat availableSpaceForNonMini = availableSpace;
  availableSpaceForNonMini -=
      [self numberOfOpenMiniTabs] * (kMiniTabWidth - kTabOverlap);

  // Initialize |nonMiniTabWidth| in case there aren't any non-mini-tabs; this
  // value shouldn't actually be used.
  CGFloat nonMiniTabWidth = kMaxTabWidth;
  CGFloat nonMiniTabWidthFraction = 0;
  const NSInteger numberOfOpenNonMiniTabs = [self numberOfOpenNonMiniTabs];
  if (numberOfOpenNonMiniTabs) {
    // Find the width of a non-mini-tab. This only applies to horizontal
    // mode. Add in the amount we "get back" from the tabs overlapping.
    availableSpaceForNonMini += (numberOfOpenNonMiniTabs - 1) * kTabOverlap;

    // Divide up the space between the non-mini-tabs.
    nonMiniTabWidth = availableSpaceForNonMini / numberOfOpenNonMiniTabs;

    // Clamp the width between the max and min.
    nonMiniTabWidth = MAX(MIN(nonMiniTabWidth, kMaxTabWidth), kMinTabWidth);

    // Separate integral and fractional parts.
    CGFloat integralPart = std::floor(nonMiniTabWidth);
    nonMiniTabWidthFraction = nonMiniTabWidth - integralPart;
    nonMiniTabWidth = integralPart;
  }

  BOOL visible = [[tabStripView_ window] isVisible];

  CGFloat offset = [self leftIndentForControls];
  bool hasPlaceholderGap = false;
  // Whether or not the last tab processed by the loop was a mini tab.
  BOOL isLastTabMini = NO;
  CGFloat tabWidthAccumulatedFraction = 0;
  NSInteger laidOutNonMiniTabs = 0;

  // Remove all the tooltip rects on the tab strip so that we can re-apply
  // them to correspond with the new tab positions.
  [tabStripView_ removeAllToolTips];

  for (TabController* tab in tabArray_.get()) {
    // Ignore a tab that is going through a close animation.
    if ([closingControllers_ containsObject:tab])
      continue;

    BOOL isPlaceholder = [[tab view] isEqual:placeholderTab_];
    NSRect tabFrame = [[tab view] frame];
    tabFrame.size.height = [[self class] defaultTabHeight];
    tabFrame.origin.y = 0;
    tabFrame.origin.x = offset;

    // If the tab is hidden, we consider it a new tab. We make it visible
    // and animate it in.
    BOOL newTab = [[tab view] isHidden];
    if (newTab)
      [[tab view] setHidden:NO];

    if (isPlaceholder) {
      // Move the current tab to the correct location instantly.
      // We need a duration or else it doesn't cancel an inflight animation.
      ScopedNSAnimationContextGroup localAnimationGroup(animate);
      localAnimationGroup.SetCurrentContextShortestDuration();
      tabFrame.origin.x = placeholderFrame_.origin.x;
      id target = animate ? [[tab view] animator] : [tab view];
      [target setFrame:tabFrame];

      // Store the frame by identifier to avoid redundant calls to animator.
      NSValue* identifier = [NSValue valueWithPointer:[tab view]];
      [targetFrames_ setObject:[NSValue valueWithRect:tabFrame]
                        forKey:identifier];
      continue;
    }

    if (placeholderTab_ && !hasPlaceholderGap) {
      const CGFloat placeholderMin = NSMinX(placeholderFrame_);
      // If the left edge is to the left of the placeholder's left, but the
      // mid is to the right of it slide over to make space for it.
      if (NSMidX(tabFrame) > placeholderMin) {
        hasPlaceholderGap = true;
        offset += NSWidth(placeholderFrame_);
        offset -= kTabOverlap;
        tabFrame.origin.x = offset;
      }
    }

    // Set the width. Selected tabs are slightly wider when things get really
    // small and thus we enforce a different minimum width.
    BOOL isMini = [tab mini];
    if (isMini) {
      tabFrame.size.width = [tab app] ? kAppTabWidth : kMiniTabWidth;
    } else {
      // Tabs have non-integer widths. Assign the integer part to the tab, and
      // keep an accumulation of the fractional parts. When the fractional
      // accumulation gets to be more than one pixel, assign that to the current
      // tab being laid out. This is vaguely inspired by Bresenham's line
      // algorithm.
      tabFrame.size.width = nonMiniTabWidth;
      tabWidthAccumulatedFraction += nonMiniTabWidthFraction;

      if (tabWidthAccumulatedFraction >= 1.0) {
        ++tabFrame.size.width;
        --tabWidthAccumulatedFraction;
      }

      // In case of rounding error, give any left over pixels to the last tab.
      if (laidOutNonMiniTabs == numberOfOpenNonMiniTabs - 1 &&
          tabWidthAccumulatedFraction > 0.5) {
        ++tabFrame.size.width;
      }

      ++laidOutNonMiniTabs;
    }

    if ([tab selected])
      tabFrame.size.width = MAX(tabFrame.size.width, kMinSelectedTabWidth);

    // If this is the first non-mini tab, then add a bit of spacing between this
    // and the last mini tab.
    if (!isMini && isLastTabMini) {
      offset += kLastMiniTabSpacing;
      tabFrame.origin.x = offset;
    }
    isLastTabMini = isMini;

    // Animate a new tab in by putting it below the horizon unless told to put
    // it in a specific location (i.e., from a drop).
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

    offset += NSWidth(tabFrame);
    offset -= kTabOverlap;

    // Create a rect which starts at the point where the tab overlap will end so
    // that as the mouse cursor crosses over the boundary it will get updated.
    // The inset is based on a multiplier of the height.
    float insetWidth = NSHeight(tabFrame) * [TabView insetMultiplier];
    // NSInsetRect will also expose the "insetWidth" at the right of the tab.
    NSRect tabToolTipRect = NSInsetRect(tabFrame, insetWidth, 0);
    [tabStripView_ addToolTipRect:tabToolTipRect owner:self userData:nil];

    // Also create two more rects in the remaining space so that the tooltip
    // is more likely to get updated crossing tabs.
    // These rects "cover" the right edge of the previous tab that was exposed
    // since the tabs overlap.
    tabToolTipRect = tabFrame;
    tabToolTipRect.size.width = insetWidth / 2.0;
    [tabStripView_ addToolTipRect:tabToolTipRect owner:self userData:nil];

    tabToolTipRect = NSOffsetRect(tabToolTipRect, insetWidth / 2.0, 0);
    [tabStripView_ addToolTipRect:tabToolTipRect owner:self userData:nil];
  }

  // Hide the new tab button if we're explicitly told to. It may already
  // be hidden, doing it again doesn't hurt. Otherwise position it
  // appropriately, showing it if necessary.
  if (forceNewTabButtonHidden_) {
    [newTabButton_ setHidden:YES];
  } else {
    NSRect newTabNewFrame = [newTabButton_ frame];
    // We've already ensured there's enough space for the new tab button
    // so we don't have to check it against the available space. We do need
    // to make sure we put it after any placeholder.
    CGFloat maxTabX = MAX(offset, NSMaxX(placeholderFrame_) - kTabOverlap);
    newTabNewFrame.origin = NSMakePoint(maxTabX + kNewTabButtonOffset, 0);
    if ([tabContentsArray_ count])
      [newTabButton_ setHidden:NO];

    if (!NSEqualRects(newTabTargetFrame_, newTabNewFrame)) {
      // Set the new tab button image correctly based on where the cursor is.
      NSWindow* window = [tabStripView_ window];
      NSPoint currentMouse = [window mouseLocationOutsideOfEventStream];
      currentMouse = [tabStripView_ convertPoint:currentMouse fromView:nil];

      BOOL shouldShowHover = [newTabButton_ pointIsOverButton:currentMouse];
      [self setNewTabButtonHoverState:shouldShowHover];

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

  // Add a catch-all tooltip rect which will handle any remaining tab strip
  // region not covered by tab-specific rects.
  [tabStripView_ addToolTipRect:enclosingRect owner:self userData:nil];

  // Mark that we've successfully completed layout of at least one tab.
  initialLayoutComplete_ = YES;
}

// Return the current hovered tab's tooltip when requested by the tooltip
// manager.
- (NSString*) view:(NSView*)view
  stringForToolTip:(NSToolTipTag)tag
             point:(NSPoint)point
          userData:(void*)data {
  return [hoveredTab_ toolTipText];
}

// When we're told to layout from the public API we usually want to animate,
// except when it's the first time.
- (void)layoutTabs {
  [self layoutTabsWithAnimation:initialLayoutComplete_ regenerateSubviews:YES];
}

- (void)layoutTabsWithoutAnimation {
  [self layoutTabsWithAnimation:NO regenerateSubviews:YES];
}

// Handles setting the title of the tab based on the given |contents|. Uses
// a canned string if |contents| is NULL.
- (void)setTabTitle:(TabController*)tab withContents:(WebContents*)contents {
  base::string16 title;
  if (contents)
    title = contents->GetTitle();
  if (title.empty())
    title = l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  [tab setTitle:base::SysUTF16ToNSString(title)];

  const base::string16& toolTip = chrome::AssembleTabTooltipText(
      title, chrome::GetTabMediaStateForContents(contents));
  [tab setToolTip:base::SysUTF16ToNSString(toolTip)];
}

// Called when a notification is received from the model to insert a new tab
// at |modelIndex|.
- (void)insertTabWithContents:(content::WebContents*)contents
                      atIndex:(NSInteger)modelIndex
                 inForeground:(bool)inForeground {
  DCHECK(contents);
  DCHECK(modelIndex == TabStripModel::kNoTab ||
         tabStripModel_->ContainsIndex(modelIndex));

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  // Make a new tab. Load the contents of this tab from the nib and associate
  // the new controller with |contents| so it can be looked up later.
  base::scoped_nsobject<TabContentsController> contentsController(
      [[TabContentsController alloc] initWithContents:contents]);
  [tabContentsArray_ insertObject:contentsController atIndex:index];

  // Make a new tab and add it to the strip. Keep track of its controller.
  TabController* newController = [self newTab];
  [newController setMini:tabStripModel_->IsMiniTab(modelIndex)];
  [newController setPinned:tabStripModel_->IsTabPinned(modelIndex)];
  [newController setApp:tabStripModel_->IsAppTab(modelIndex)];
  [newController setUrl:contents->GetURL()];
  [tabArray_ insertObject:newController atIndex:index];
  NSView* newView = [newController view];

  // Set the originating frame to just below the strip so that it animates
  // upwards as it's being initially layed out. Oddly, this works while doing
  // something similar in |-layoutTabs| confuses the window server.
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
  [self updateIconsForContents:contents atIndex:modelIndex];
}

// Called before |contents| is deactivated.
- (void)tabDeactivatedWithContents:(content::WebContents*)contents {
  contents->StoreFocus();
}

// Called when a notification is received from the model to select a particular
// tab. Swaps in the toolbar and content area associated with |newContents|.
- (void)activateTabWithContents:(content::WebContents*)newContents
               previousContents:(content::WebContents*)oldContents
                        atIndex:(NSInteger)modelIndex
                         reason:(int)reason {
  // Take closing tabs into account.
  if (oldContents) {
    int oldModelIndex =
        browser_->tab_strip_model()->GetIndexOfWebContents(oldContents);
    if (oldModelIndex != -1) {  // When closing a tab, the old tab may be gone.
      NSInteger oldIndex = [self indexFromModelIndex:oldModelIndex];
      TabContentsController* oldController =
          [tabContentsArray_ objectAtIndex:oldIndex];
      [oldController willBecomeUnselectedTab];
      oldContents->WasHidden();
    }
  }

  NSUInteger activeIndex = [self indexFromModelIndex:modelIndex];

  [tabArray_ enumerateObjectsUsingBlock:^(TabController* current,
                                          NSUInteger index,
                                          BOOL* stop) {
      [current setActive:index == activeIndex];
  }];

  // Tell the new tab contents it is about to become the selected tab. Here it
  // can do things like make sure the toolbar is up to date.
  TabContentsController* newController =
      [tabContentsArray_ objectAtIndex:activeIndex];
  [newController willBecomeSelectedTab];

  // Relayout for new tabs and to let the selected tab grow to be larger in
  // size than surrounding tabs if the user has many. This also raises the
  // selected tab to the top.
  [self layoutTabs];

  // Swap in the contents for the new tab.
  [self swapInTabAtIndex:modelIndex];

  if (newContents) {
    newContents->WasShown();
    newContents->RestoreFocus();
  }
}

- (void)tabSelectionChanged {
  // First get the vector of indices, which is allays sorted in ascending order.
  ui::ListSelectionModel::SelectedIndices selection(
      tabStripModel_->selection_model().selected_indices());
  // Iterate through all of the tabs, selecting each as necessary.
  ui::ListSelectionModel::SelectedIndices::iterator iter = selection.begin();
  int i = 0;
  for (TabController* current in tabArray_.get()) {
    BOOL selected = iter != selection.end() &&
        [self indexFromModelIndex:*iter] == i;
    [current setSelected:selected];
    if (selected)
      ++iter;
    ++i;
  }
}

- (void)tabReplacedWithContents:(content::WebContents*)newContents
               previousContents:(content::WebContents*)oldContents
                        atIndex:(NSInteger)modelIndex {
  NSInteger index = [self indexFromModelIndex:modelIndex];
  TabContentsController* oldController =
      [tabContentsArray_ objectAtIndex:index];
  DCHECK_EQ(oldContents, [oldController webContents]);

  // Simply create a new TabContentsController for |newContents| and place it
  // into the array, replacing |oldContents|.  An ActiveTabChanged notification
  // will follow, at which point we will install the new view.
  base::scoped_nsobject<TabContentsController> newController(
      [[TabContentsController alloc] initWithContents:newContents]);

  // Bye bye, |oldController|.
  [tabContentsArray_ replaceObjectAtIndex:index withObject:newController];

  // Fake a tab changed notification to force tab titles and favicons to update.
  [self tabChangedWithContents:newContents
                       atIndex:modelIndex
                    changeType:TabStripModelObserver::ALL];
}

// Remove all knowledge about this tab and its associated controller, and remove
// the view from the strip.
- (void)removeTab:(TabController*)controller {
  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  NSUInteger index = [tabArray_ indexOfObject:controller];

  // Release the tab contents controller so those views get destroyed. This
  // will remove all the tab content Cocoa views from the hierarchy. A
  // subsequent "select tab" notification will follow from the model. To
  // tell us what to swap in in its absence.
  [tabContentsArray_ removeObjectAtIndex:index];

  // Remove the view from the tab strip.
  NSView* tab = [controller view];
  [tab removeFromSuperview];

  // Remove ourself as an observer.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSViewDidUpdateTrackingAreasNotification
              object:tab];

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
- (void)animationDidStop:(CAAnimation*)animation
           forController:(TabController*)controller
                finished:(BOOL)finished{
  [[animation delegate] invalidate];
  [closingControllers_ removeObject:controller];
  [self removeTab:controller];
}

// Save off which TabController is closing and tell its view's animator
// where to move the tab to. Registers a delegate to call back when the
// animation is complete in order to remove the tab from the model.
- (void)startClosingTabWithAnimation:(TabController*)closingTab {
  DCHECK([NSThread isMainThread]);

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  // Save off the controller into the set of animating tabs. This alerts
  // the layout method to not do anything with it and allows us to correctly
  // calculate offsets when working with indices into the model.
  [closingControllers_ addObject:closingTab];

  // Mark the tab as closing. This prevents it from generating any drags or
  // selections while it's animating closed.
  [[closingTab tabView] setClosing:YES];

  // Register delegate (owned by the animation system).
  NSView* tabView = [closingTab view];
  CAAnimation* animation = [[tabView animationForKey:@"frameOrigin"] copy];
  [animation autorelease];
  base::scoped_nsobject<TabCloseAnimationDelegate> delegate(
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
- (void)tabDetachedWithContents:(content::WebContents*)contents
                        atIndex:(NSInteger)modelIndex {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  TabController* tab = [tabArray_ objectAtIndex:index];
  if (tabStripModel_->count() > 0) {
    [self startClosingTabWithAnimation:tab];
    [self layoutTabs];
  } else {
    // Don't remove the tab, as that makes the window look jarring without any
    // tabs. Instead, simply mark it as closing to prevent the tab from
    // generating any drags or selections.
    [[tab tabView] setClosing:YES];
  }

  [delegate_ onTabDetachedWithContents:contents];
}

// A helper routine for creating an NSImageView to hold the favicon or app icon
// for |contents|.
- (NSImage*)iconImageForContents:(content::WebContents*)contents {
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  BOOL isApp = extensions_tab_helper->is_app();
  NSImage* image = nil;
  // Favicons come from the renderer, and the renderer draws everything in the
  // system color space.
  CGColorSpaceRef colorSpace = base::mac::GetSystemColorSpace();
  if (isApp) {
    SkBitmap* icon = extensions_tab_helper->GetExtensionAppIcon();
    if (icon)
      image = gfx::SkBitmapToNSImageWithColorSpace(*icon, colorSpace);
  } else {
    image = mac::FaviconForWebContents(contents);
  }

  // Either we don't have a valid favicon or there was some issue converting it
  // from an SkBitmap. Either way, just show the default.
  if (!image)
    image = defaultFavicon_.get();

  return image;
}

// Updates the current loading state, replacing the icon view with a favicon,
// a throbber, the default icon, or nothing at all.
- (void)updateIconsForContents:(content::WebContents*)contents
                       atIndex:(NSInteger)modelIndex {
  if (!contents)
    return;

  static NSImage* throbberWaitingImage =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_THROBBER_WAITING).CopyNSImage();
  static NSImage* throbberLoadingImage =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_THROBBER).CopyNSImage();
  static NSImage* sadFaviconImage =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_SAD_FAVICON).CopyNSImage();

  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];
  TabController* tabController = [tabArray_ objectAtIndex:index];

  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(contents);
  bool oldHasIcon = [tabController iconView] != nil;
  bool newHasIcon = favicon_tab_helper->ShouldDisplayFavicon() ||
      tabStripModel_->IsMiniTab(modelIndex);  // Always show icon if mini.

  TabLoadingState oldState = [tabController loadingState];
  TabLoadingState newState = kTabDone;
  NSImage* throbberImage = nil;
  if (contents->IsCrashed()) {
    newState = kTabCrashed;
    newHasIcon = true;
  } else if (contents->IsWaitingForResponse()) {
    newState = kTabWaiting;
    throbberImage = throbberWaitingImage;
  } else if (contents->IsLoadingToDifferentDocument()) {
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
    if (newHasIcon) {
      if (newState == kTabDone) {
        [tabController setIconImage:[self iconImageForContents:contents]];
        const TabMediaState mediaState =
            chrome::GetTabMediaStateForContents(contents);
        // Create MediaIndicatorView upon first use.
        if (mediaState != TAB_MEDIA_STATE_NONE &&
            ![tabController mediaIndicatorView]) {
          MediaIndicatorView* const mediaIndicatorView =
              [[[MediaIndicatorView alloc] init] autorelease];
          [tabController setMediaIndicatorView:mediaIndicatorView];
        }
        [[tabController mediaIndicatorView] updateIndicator:mediaState];
      } else if (newState == kTabCrashed) {
        [tabController setIconImage:sadFaviconImage withToastAnimation:YES];
        [[tabController mediaIndicatorView]
          updateIndicator:TAB_MEDIA_STATE_NONE];
      } else {
        [tabController setIconImage:throbberImage];
      }
    } else {
      [tabController setIconImage:nil];
    }
  }
}

// Called when a notification is received from the model that the given tab
// has been updated. |loading| will be YES when we only want to update the
// throbber state, not anything else about the (partially) loading tab.
- (void)tabChangedWithContents:(content::WebContents*)contents
                       atIndex:(NSInteger)modelIndex
                    changeType:(TabStripModelObserver::TabChangeType)change {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  if (modelIndex == tabStripModel_->active_index())
    [delegate_ onTabChanged:change withContents:contents];

  if (change == TabStripModelObserver::TITLE_NOT_LOADING) {
    // TODO(sky): make this work.
    // We'll receive another notification of the change asynchronously.
    return;
  }

  TabController* tabController = [tabArray_ objectAtIndex:index];

  if (change != TabStripModelObserver::LOADING_ONLY)
    [self setTabTitle:tabController withContents:contents];

  [self updateIconsForContents:contents atIndex:modelIndex];

  TabContentsController* updatedController =
      [tabContentsArray_ objectAtIndex:index];
  [updatedController tabDidChange:contents];
}

// Called when a tab is moved (usually by drag&drop). Keep our parallel arrays
// in sync with the tab strip model. It can also be pinned/unpinned
// simultaneously, so we need to take care of that.
- (void)tabMovedWithContents:(content::WebContents*)contents
                   fromIndex:(NSInteger)modelFrom
                     toIndex:(NSInteger)modelTo {
  // Take closing tabs into account.
  NSInteger from = [self indexFromModelIndex:modelFrom];
  NSInteger to = [self indexFromModelIndex:modelTo];

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  base::scoped_nsobject<TabContentsController> movedTabContentsController(
      [[tabContentsArray_ objectAtIndex:from] retain]);
  [tabContentsArray_ removeObjectAtIndex:from];
  [tabContentsArray_ insertObject:movedTabContentsController.get()
                          atIndex:to];
  base::scoped_nsobject<TabController> movedTabController(
      [[tabArray_ objectAtIndex:from] retain]);
  DCHECK([movedTabController isKindOfClass:[TabController class]]);
  [tabArray_ removeObjectAtIndex:from];
  [tabArray_ insertObject:movedTabController.get() atIndex:to];

  // The tab moved, which means that the mini-tab state may have changed.
  if (tabStripModel_->IsMiniTab(modelTo) != [movedTabController mini])
    [self tabMiniStateChangedWithContents:contents atIndex:modelTo];

  [self layoutTabs];
}

// Called when a tab is pinned or unpinned without moving.
- (void)tabMiniStateChangedWithContents:(content::WebContents*)contents
                                atIndex:(NSInteger)modelIndex {
  // Take closing tabs into account.
  NSInteger index = [self indexFromModelIndex:modelIndex];

  TabController* tabController = [tabArray_ objectAtIndex:index];
  DCHECK([tabController isKindOfClass:[TabController class]]);

  // Don't do anything if the change was already picked up by the move event.
  if (tabStripModel_->IsMiniTab(modelIndex) == [tabController mini])
    return;

  [tabController setMini:tabStripModel_->IsMiniTab(modelIndex)];
  [tabController setPinned:tabStripModel_->IsTabPinned(modelIndex)];
  [tabController setApp:tabStripModel_->IsAppTab(modelIndex)];
  [tabController setUrl:contents->GetURL()];
  [self updateIconsForContents:contents atIndex:modelIndex];
  // If the tab is being restored and it's pinned, the mini state is set after
  // the tab has already been rendered, so re-layout the tabstrip. In all other
  // cases, the state is set before the tab is rendered so this isn't needed.
  [self layoutTabs];
}

- (void)setFrame:(NSRect)frame ofTabView:(NSView*)view {
  NSValue* identifier = [NSValue valueWithPointer:view];
  [targetFrames_ setObject:[NSValue valueWithRect:frame]
                    forKey:identifier];
  [view setFrame:frame];
}

- (TabStripModel*)tabStripModel {
  return tabStripModel_;
}

- (NSArray*)tabViews {
  NSMutableArray* views = [NSMutableArray arrayWithCapacity:[tabArray_ count]];
  for (TabController* tab in tabArray_.get()) {
    [views addObject:[tab tabView]];
  }
  return views;
}

- (NSView*)activeTabView {
  int activeIndex = tabStripModel_->active_index();
  // Take closing tabs into account. They can't ever be selected.
  activeIndex = [self indexFromModelIndex:activeIndex];
  return [self viewAtIndex:activeIndex];
}

- (int)indexOfPlaceholder {
  // Use |tabArray_| here instead of the tab strip count in order to get the
  // correct index when there are closing tabs to the left of the placeholder.
  const int count = [tabArray_ count];

  // No placeholder, return the end of the strip.
  if (placeholderTab_ == nil)
    return count;

  double placeholderX = placeholderFrame_.origin.x;
  int index = 0;
  int location = 0;
  while (index < count) {
    // Ignore closing tabs for simplicity. The only drawback of this is that
    // if the placeholder is placed right before one or several contiguous
    // currently closing tabs, the associated TabController will start at the
    // end of the closing tabs.
    if ([closingControllers_ containsObject:[tabArray_ objectAtIndex:index]]) {
      index++;
      continue;
    }
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
  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();
  tabStripModel_->MoveWebContentsAt(from, toIndex, true);
}

// Drop a given WebContents at the location of the current placeholder.
// If there is no placeholder, it will go at the end. Used when dragging from
// another window when we don't have access to the WebContents as part of our
// strip. |frame| is in the coordinate system of the tab strip view and
// represents where the user dropped the new tab so it can be animated into its
// correct location when the tab is added to the model. If the tab was pinned in
// its previous window, setting |pinned| to YES will propagate that state to the
// new window. Mini-tabs are either app or pinned tabs; the app state is stored
// by the |contents|, but the |pinned| state is the caller's responsibility.
- (void)dropWebContents:(WebContents*)contents
                atIndex:(int)modelIndex
              withFrame:(NSRect)frame
            asPinnedTab:(BOOL)pinned
               activate:(BOOL)activate {
  // Mark that the new tab being created should start at |frame|. It will be
  // reset as soon as the tab has been positioned.
  droppedTabFrame_ = frame;

  // Insert it into this tab strip. We want it in the foreground and to not
  // inherit the current tab's group.
  tabStripModel_->InsertWebContentsAt(
      modelIndex,
      contents,
      (activate ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE) |
          (pinned ? TabStripModel::ADD_PINNED : TabStripModel::ADD_NONE));
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
  NSWindow* window = [tabStripView_ window];
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  if (NSPointInRect(location, [tabStripView_ frame])) {
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
  NSView* targetView = [tabStripView_ hitTest:[event locationInWindow]];

  // Set the new tab button hover state iff the mouse is over the button.
  BOOL shouldShowHoverImage = [targetView isKindOfClass:[NewTabButton class]];
  [self setNewTabButtonHoverState:shouldShowHoverImage];

  TabView* tabView = (TabView*)targetView;
  if (![tabView isKindOfClass:[TabView class]]) {
    if ([[tabView superview] isKindOfClass:[TabView class]]) {
      tabView = (TabView*)[targetView superview];
    } else {
      tabView = nil;
    }
  }

  if (hoveredTab_ != tabView) {
    [hoveredTab_ mouseExited:nil];  // We don't pass event because moved events
    [tabView mouseEntered:nil];  // don't have valid tracking areas
    hoveredTab_ = tabView;
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
    // If the mouse is moved quickly enough, it is possible for the mouse to
    // leave the tabstrip without sending any mouseMoved: messages at all.
    // Since this would result in the new tab button incorrectly staying in the
    // hover state, disable the hover image on every mouse exit.
    [self setNewTabButtonHoverState:NO];
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

// Sets the new tab button's image based on the current hover state.  Does
// nothing if the hover state is already correct.
- (void)setNewTabButtonHoverState:(BOOL)shouldShowHover {
  if (shouldShowHover && !newTabButtonShowingHoverImage_) {
    newTabButtonShowingHoverImage_ = YES;
    [[newTabButton_ cell] setIsMouseInside:YES];
  } else if (!shouldShowHover && newTabButtonShowingHoverImage_) {
    newTabButtonShowingHoverImage_ = NO;
    [[newTabButton_ cell] setIsMouseInside:NO];
  }
}

// Adds the given subview to (the end of) the list of permanent subviews
// (specified from bottom up). These subviews will always be below the
// transitory subviews (tabs). |-regenerateSubviewList| must be called to
// effectuate the addition.
- (void)addSubviewToPermanentList:(NSView*)aView {
  if (aView)
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

  NSView* activeTabView = nil;
  // Go through tabs in reverse order, since |subviews| is bottom-to-top.
  for (TabController* tab in [tabArray_ reverseObjectEnumerator]) {
    NSView* tabView = [tab view];
    if ([tab active]) {
      DCHECK(!activeTabView);
      activeTabView = tabView;
    } else {
      [subviews addObject:tabView];
    }
  }
  if (activeTabView) {
    [subviews addObject:activeTabView];
  }
  WithNoAnimation noAnimation;
  [tabStripView_ setSubviews:subviews];
  [self setTabTrackingAreasEnabled:mouseInside_];
}

// Get the index and disposition for a potential URL(s) drop given a point (in
// the |TabStripView|'s coordinates). It considers only the x-coordinate of the
// given point. If it's in the "middle" of a tab, it drops on that tab. If it's
// to the left, it inserts to the left, and similarly for the right.
- (void)droppingURLsAt:(NSPoint)point
            givesIndex:(NSInteger*)index
           disposition:(WindowOpenDisposition*)disposition {
  // Proportion of the tab which is considered the "middle" (and causes things
  // to drop on that tab).
  const double kMiddleProportion = 0.5;
  const double kLRProportion = (1.0 - kMiddleProportion) / 2.0;

  DCHECK(index && disposition);
  NSInteger i = 0;
  for (TabController* tab in tabArray_.get()) {
    NSView* view = [tab view];
    DCHECK([view isKindOfClass:[TabView class]]);

    // Recall that |-[NSView frame]| is in its superview's coordinates, so a
    // |TabView|'s frame is in the coordinates of the |TabStripView| (which
    // matches the coordinate system of |point|).
    NSRect frame = [view frame];

    // Modify the frame to make it "unoverlapped".
    frame.origin.x += kTabOverlap / 2.0;
    frame.size.width -= kTabOverlap;
    if (frame.size.width < 1.0)
      frame.size.width = 1.0;  // try to avoid complete failure

    // Drop in a new tab to the left of tab |i|?
    if (point.x < (frame.origin.x + kLRProportion * frame.size.width)) {
      *index = i;
      *disposition = NEW_FOREGROUND_TAB;
      return;
    }

    // Drop on tab |i|?
    if (point.x <= (frame.origin.x +
                       (1.0 - kLRProportion) * frame.size.width)) {
      *index = i;
      *disposition = CURRENT_TAB;
      return;
    }

    // (Dropping in a new tab to the right of tab |i| will be taken care of in
    // the next iteration.)
    i++;
  }

  // If we've made it here, we want to append a new tab to the end.
  *index = -1;
  *disposition = NEW_FOREGROUND_TAB;
}

- (void)openURL:(GURL*)url inView:(NSView*)view at:(NSPoint)point {
  // Get the index and disposition.
  NSInteger index;
  WindowOpenDisposition disposition;
  [self droppingURLsAt:point
            givesIndex:&index
           disposition:&disposition];

  // Either insert a new tab or open in a current tab.
  switch (disposition) {
    case NEW_FOREGROUND_TAB: {
      content::RecordAction(UserMetricsAction("Tab_DropURLBetweenTabs"));
      chrome::NavigateParams params(browser_, *url,
                                    content::PAGE_TRANSITION_TYPED);
      params.disposition = disposition;
      params.tabstrip_index = index;
      params.tabstrip_add_types =
          TabStripModel::ADD_ACTIVE | TabStripModel::ADD_FORCE_INDEX;
      chrome::Navigate(&params);
      break;
    }
    case CURRENT_TAB: {
      content::RecordAction(UserMetricsAction("Tab_DropURLOnTab"));
      OpenURLParams params(
          *url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false);
      tabStripModel_->GetWebContentsAt(index)->OpenURL(params);
      tabStripModel_->ActivateTabAt(index, true);
      break;
    }
    default:
      NOTIMPLEMENTED();
  }
}

// (URLDropTargetController protocol)
- (void)dropURLs:(NSArray*)urls inView:(NSView*)view at:(NSPoint)point {
  DCHECK_EQ(view, tabStripView_.get());

  if ([urls count] < 1) {
    NOTREACHED();
    return;
  }

  //TODO(viettrungluu): dropping multiple URLs.
  if ([urls count] > 1)
    NOTIMPLEMENTED();

  // Get the first URL and fix it up.
  GURL url(GURL(url_fixer::FixupURL(
      base::SysNSStringToUTF8([urls objectAtIndex:0]), std::string())));

  [self openURL:&url inView:view at:point];
}

// (URLDropTargetController protocol)
- (void)dropText:(NSString*)text inView:(NSView*)view at:(NSPoint)point {
  DCHECK_EQ(view, tabStripView_.get());

  // If the input is plain text, classify the input and make the URL.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_->profile())->Classify(
      base::SysNSStringToUTF16(text), false, false,
      metrics::OmniboxEventProto::BLANK, &match, NULL);
  GURL url(match.destination_url);

  [self openURL:&url inView:view at:point];
}

// (URLDropTargetController protocol)
- (void)indicateDropURLsInView:(NSView*)view at:(NSPoint)point {
  DCHECK_EQ(view, tabStripView_.get());

  // The minimum y-coordinate at which one should consider place the arrow.
  const CGFloat arrowBaseY = 25;

  NSInteger index;
  WindowOpenDisposition disposition;
  [self droppingURLsAt:point
            givesIndex:&index
           disposition:&disposition];

  NSPoint arrowPos = NSMakePoint(0, arrowBaseY);
  if (index == -1) {
    // Append a tab at the end.
    DCHECK(disposition == NEW_FOREGROUND_TAB);
    NSInteger lastIndex = [tabArray_ count] - 1;
    NSRect overRect = [[[tabArray_ objectAtIndex:lastIndex] view] frame];
    arrowPos.x = overRect.origin.x + overRect.size.width - kTabOverlap / 2.0;
  } else {
    NSRect overRect = [[[tabArray_ objectAtIndex:index] view] frame];
    switch (disposition) {
      case NEW_FOREGROUND_TAB:
        // Insert tab (to the left of the given tab).
        arrowPos.x = overRect.origin.x + kTabOverlap / 2.0;
        break;
      case CURRENT_TAB:
        // Overwrite the given tab.
        arrowPos.x = overRect.origin.x + overRect.size.width / 2.0;
        break;
      default:
        NOTREACHED();
    }
  }

  [tabStripView_ setDropArrowPosition:arrowPos];
  [tabStripView_ setDropArrowShown:YES];
  [tabStripView_ setNeedsDisplay:YES];

  // Perform a delayed tab transition if hovering directly over a tab.
  if (index != -1 && disposition == CURRENT_TAB) {
    NSInteger modelIndex = [self modelIndexFromIndex:index];
    // Only start the transition if it has a valid model index (i.e. it's not
    // in the middle of closing).
    if (modelIndex != NSNotFound) {
      hoverTabSelector_->StartTabTransition(modelIndex);
      return;
    }
  }
  // If a tab transition was not started, cancel the pending one.
  hoverTabSelector_->CancelTabTransition();
}

// (URLDropTargetController protocol)
- (void)hideDropURLsIndicatorInView:(NSView*)view {
  DCHECK_EQ(view, tabStripView_.get());

  // Cancel any pending tab transition.
  hoverTabSelector_->CancelTabTransition();

  if ([tabStripView_ dropArrowShown]) {
    [tabStripView_ setDropArrowShown:NO];
    [tabStripView_ setNeedsDisplay:YES];
  }
}

// (URLDropTargetController protocol)
- (BOOL)isUnsupportedDropData:(id<NSDraggingInfo>)info {
  return drag_util::IsUnsupportedDropData(browser_->profile(), info);
}

- (TabContentsController*)activeTabContentsController {
  int modelIndex = tabStripModel_->active_index();
  if (modelIndex < 0)
    return nil;
  NSInteger index = [self indexFromModelIndex:modelIndex];
  if (index < 0 ||
      index >= (NSInteger)[tabContentsArray_ count])
    return nil;
  return [tabContentsArray_ objectAtIndex:index];
}

- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self setNewTabImages];
}

- (void)setNewTabImages {
  ThemeService *theme =
      static_cast<ThemeService*>([[tabStripView_ window] themeProvider]);
  if (!theme)
    return;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* mask = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_MASK).ToNSImage();
  NSImage* normal = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON).ToNSImage();
  NSImage* hover = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_H).ToNSImage();
  NSImage* pressed = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_P).ToNSImage();

  NSImage* foreground = ApplyMask(
      theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND), mask);

  [[newTabButton_ cell] setImage:Overlay(foreground, normal, 1.0)
                  forButtonState:image_button_cell::kDefaultState];
  [[newTabButton_ cell] setImage:Overlay(foreground, hover, 1.0)
                  forButtonState:image_button_cell::kHoverState];
  [[newTabButton_ cell] setImage:Overlay(foreground, pressed, 1.0)
                    forButtonState:image_button_cell::kPressedState];

  // IDR_THEME_TAB_BACKGROUND_INACTIVE is only used with the default theme.
  if (theme->UsingDefaultTheme()) {
    const CGFloat alpha = tabs::kImageNoFocusAlpha;
    NSImage* background = ApplyMask(
        theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND_INACTIVE), mask);
    [[newTabButton_ cell] setImage:Overlay(background, normal, alpha)
                    forButtonState:image_button_cell::kDefaultStateBackground];
    [[newTabButton_ cell] setImage:Overlay(background, hover, alpha)
                    forButtonState:image_button_cell::kHoverStateBackground];
  } else {
    [[newTabButton_ cell] setImage:nil
                    forButtonState:image_button_cell::kDefaultStateBackground];
    [[newTabButton_ cell] setImage:nil
                    forButtonState:image_button_cell::kHoverStateBackground];
  }
}

@end

NSView* GetSheetParentViewForWebContents(WebContents* web_contents) {
  // View hierarchy of the contents view:
  // NSView  -- switchView, same for all tabs
  // +- NSView  -- TabContentsController's view
  //    +- TabContentsViewCocoa
  //
  // Changing it? Do not forget to modify
  // -[TabStripController swapInTabAtIndex:] too.
  return [web_contents->GetNativeView() superview];
}
