// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_background_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/theme_provider.h"

@interface TabWindowController ()
- (void)setUseOverlay:(BOOL)useOverlay;

// The tab strip background view should always be inserted as the back-most
// subview of the root view. It cannot be a subview of the contentView, as that
// would cause it to become layer backed, which would cause it to draw on top
// of non-layer backed content like the window controls.
- (void)insertTabStripBackgroundViewIntoWindow:(NSWindow*)window;
@end

@interface TabWindowOverlayWindow : NSWindow
@end

@implementation TabWindowOverlayWindow

- (ui::ThemeProvider*)themeProvider {
  if ([self parentWindow])
    return [[[self parentWindow] windowController] themeProvider];
  return NULL;
}

- (ThemedWindowStyle)themedWindowStyle {
  if ([self parentWindow])
    return [[[self parentWindow] windowController] themedWindowStyle];
  return NO;
}

- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment {
  if ([self parentWindow]) {
    return [[[self parentWindow] windowController]
        themeImagePositionForAlignment:alignment];
  }
  return NSZeroPoint;
}

@end

@implementation TabWindowController

- (id)initTabWindowControllerWithTabStrip:(BOOL)hasTabStrip {
  const CGFloat kDefaultWidth = 750;
  const CGFloat kDefaultHeight = 600;

  NSRect contentRect = NSMakeRect(60, 229, kDefaultWidth, kDefaultHeight);
  base::scoped_nsobject<FramedBrowserWindow> window(
      [[FramedBrowserWindow alloc] initWithContentRect:contentRect
                                           hasTabStrip:hasTabStrip]);
  [window setReleasedWhenClosed:YES];
  [window setAutorecalculatesKeyViewLoop:YES];

  if ((self = [super initWithWindow:window])) {
    [[self window] setDelegate:self];

    chromeContentView_.reset([[NSView alloc]
        initWithFrame:NSMakeRect(0, 0, kDefaultWidth, kDefaultHeight)]);
    [chromeContentView_
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [chromeContentView_ setWantsLayer:YES];
    [[[self window] contentView] addSubview:chromeContentView_];

    tabContentArea_.reset(
        [[FastResizeView alloc] initWithFrame:[chromeContentView_ bounds]]);
    [tabContentArea_ setAutoresizingMask:NSViewWidthSizable |
                                         NSViewHeightSizable];
    [chromeContentView_ addSubview:tabContentArea_];

    // tabStripBackgroundView_ draws the theme image behind the tab strip area.
    // When making a tab dragging window (setUseOverlay:), this view stays in
    // the parent window so that it can be translucent, while the tab strip view
    // moves to the child window and stays opaque.
    NSView* windowView = [window contentView];
    tabStripBackgroundView_.reset([[TabStripBackgroundView alloc]
        initWithFrame:NSMakeRect(0,
                                 NSMaxY([windowView bounds]) -
                                     kBrowserFrameViewPaintHeight,
                                 NSWidth([windowView bounds]),
                                 kBrowserFrameViewPaintHeight)]);
    [tabStripBackgroundView_
        setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [self insertTabStripBackgroundViewIntoWindow:window];

    tabStripView_.reset([[TabStripView alloc]
        initWithFrame:NSMakeRect(
                          0, 0, kDefaultWidth, chrome::kTabStripHeight)]);
    [tabStripView_ setAutoresizingMask:NSViewWidthSizable |
                                       NSViewMinYMargin];
    if (hasTabStrip)
      [windowView addSubview:tabStripView_];
  }
  return self;
}

- (NSView*)tabStripBackgroundView {
  return tabStripBackgroundView_;
}

- (TabStripView*)tabStripView {
  return tabStripView_;
}

- (FastResizeView*)tabContentArea {
  return tabContentArea_;
}

- (NSView*)chromeContentView {
  return chromeContentView_;
}

- (void)removeOverlay {
  [self setUseOverlay:NO];
  if (closeDeferred_) {
    // See comment in BrowserWindowCocoa::Close() about orderOut:.
    [[self window] orderOut:self];
    [[self window] performClose:self];  // Autoreleases the controller.
  }
}

- (void)showOverlay {
  [self setUseOverlay:YES];
}

// If |useOverlay| is YES, creates a new overlay window and puts the tab strip
// and the content area inside of it. This allows it to have a different opacity
// from the title bar. If NO, returns everything to the previous state and
// destroys the overlay window until it's needed again. The tab strip and window
// contents are returned to the original window.
- (void)setUseOverlay:(BOOL)useOverlay {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(removeOverlay)
                                             object:nil];
  NSWindow* window = [self window];
  if (useOverlay && !overlayWindow_) {
    DCHECK(!originalContentView_);

    overlayWindow_ = [[TabWindowOverlayWindow alloc]
                         initWithContentRect:[window frame]
                                   styleMask:NSBorderlessWindowMask
                                     backing:NSBackingStoreBuffered
                                       defer:NO];
    [overlayWindow_ setTitle:@"overlay"];
    [overlayWindow_ setBackgroundColor:[NSColor clearColor]];
    [overlayWindow_ setOpaque:NO];
    [overlayWindow_ setDelegate:self];
    [[overlayWindow_ contentView] setWantsLayer:YES];

    originalContentView_ = self.chromeContentView;
    [window addChildWindow:overlayWindow_ ordered:NSWindowAbove];

    // Explicitly set the responder to be nil here (for restoring later).
    // If the first responder were to be left non-nil here then
    // [RenderWidgethostViewCocoa resignFirstResponder] would be called,
    // followed by RenderWidgetHost::Blur(), which would result in an unexpected
    // loss of focus.
    focusBeforeOverlay_.reset([[FocusTracker alloc] initWithWindow:window]);
    [window makeFirstResponder:nil];

    // Move the original window's tab strip view and content view to the overlay
    // window. The content view is added as a subview of the overlay window's
    // content view (rather than using setContentView:) because the overlay
    // window has a different content size (due to it being borderless).
    [[overlayWindow_ contentView] addSubview:[self tabStripView]];
    [[overlayWindow_ contentView] addSubview:originalContentView_];

    [overlayWindow_ orderFront:nil];
  } else if (!useOverlay && overlayWindow_) {
    DCHECK(originalContentView_);

    // Return the original window's tab strip view and content view to their
    // places. The TabStripView always needs to be in front of the window's
    // content view and therefore it should always be added after the content
    // view is set.
    [[window contentView] addSubview:originalContentView_
                          positioned:NSWindowBelow
                          relativeTo:nil];
    originalContentView_.frame = [[window contentView] bounds];
    [[window contentView] addSubview:[self tabStripView]];
    [[window contentView] updateTrackingAreas];

    [focusBeforeOverlay_ restoreFocusInWindow:window];
    focusBeforeOverlay_.reset();

    [window display];
    [window removeChildWindow:overlayWindow_];

    [overlayWindow_ orderOut:nil];
    [overlayWindow_ release];
    overlayWindow_ = nil;
    originalContentView_ = nil;
  } else {
    NOTREACHED();
  }
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (BOOL)shouldConstrainFrameRect {
  // If we currently have an overlay window, do not attempt to change the
  // window's size, as our overlay window doesn't know how to resize properly.
  return overlayWindow_ == nil;
}

- (BOOL)canReceiveFrom:(TabWindowController*)source {
  // subclass must implement
  NOTIMPLEMENTED();
  return NO;
}

- (void)moveTabViews:(NSArray*)views
      fromController:(TabWindowController*)dragController {
  NOTIMPLEMENTED();
}

- (NSArray*)tabViews {
  NOTIMPLEMENTED();
  return nil;
}

- (NSView*)activeTabView {
  NOTIMPLEMENTED();
  return nil;
}

- (void)layoutTabs {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (TabWindowController*)detachTabsToNewWindow:(NSArray*)tabViews
                                   draggedTab:(NSView*)draggedTab {
  // subclass must implement
  NOTIMPLEMENTED();
  return NULL;
}

- (void)insertPlaceholderForTab:(TabView*)tab frame:(NSRect)frame {
  [self showNewTabButton:NO];
}

- (void)removePlaceholder {
  [self showNewTabButton:YES];
}

- (BOOL)isDragSessionActive {
  NOTIMPLEMENTED();
  return NO;
}

- (BOOL)tabDraggingAllowed {
  return YES;
}

- (BOOL)tabTearingAllowed {
  return YES;
}

- (BOOL)windowMovementAllowed {
  return YES;
}

- (BOOL)isTabFullyVisible:(TabView*)tab {
  // Subclasses should implement this, but it's not necessary.
  return YES;
}

- (void)showNewTabButton:(BOOL)show {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (void)detachTabView:(NSView*)view {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (NSInteger)numberOfTabs {
  // subclass must implement
  NOTIMPLEMENTED();
  return 0;
}

- (BOOL)hasLiveTabs {
  // subclass must implement
  NOTIMPLEMENTED();
  return NO;
}

- (NSString*)activeTabTitle {
  // subclass must implement
  NOTIMPLEMENTED();
  return @"";
}

- (BOOL)hasTabStrip {
  // Subclasses should implement this.
  NOTIMPLEMENTED();
  return YES;
}

- (BOOL)isTabDraggable:(NSView*)tabView {
  // Subclasses should implement this.
  NOTIMPLEMENTED();
  return YES;
}

// Tell the window that it needs to call performClose: as soon as the current
// drag is complete. This prevents a window (and its overlay) from going away
// during a drag.
- (void)deferPerformClose {
  closeDeferred_ = YES;
}

- (void)insertTabStripBackgroundViewIntoWindow:(NSWindow*)window {
  DCHECK(tabStripBackgroundView_);
  NSView* rootView = [[window contentView] superview];
  [rootView addSubview:tabStripBackgroundView_
            positioned:NSWindowBelow
            relativeTo:nil];
}

// Called when the size of the window content area has changed. Override to
// position specific views. Base class implementation does nothing.
- (void)layoutSubviews {
  NOTIMPLEMENTED();
}

@end
