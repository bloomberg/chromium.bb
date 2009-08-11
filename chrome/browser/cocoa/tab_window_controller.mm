// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_window_controller.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/tab_strip_view.h"

@interface TabWindowController(PRIVATE)
- (void)setUseOverlay:(BOOL)useOverlay;
@end

@implementation TabWindowController
@synthesize tabStripView = tabStripView_;
@synthesize tabContentArea = tabContentArea_;

- (id)initWithWindow:(NSWindow *)window {
  if ((self = [super initWithWindow:window]) != nil) {
    lockedTabs_.reset([[NSMutableSet alloc] initWithCapacity:10]);
  }
  return self;
}

- (void)windowDidLoad {
  if ([self isNormalWindow]) {
    // Place the tab bar above the content box and add it to the view hierarchy
    // as a sibling of the content view so it can overlap with the window frame.
    NSRect tabFrame = [tabContentArea_ frame];
    tabFrame.origin = NSMakePoint(0, NSMaxY(tabFrame));
    tabFrame.size.height = NSHeight([tabStripView_ frame]);
    [tabStripView_ setFrame:tabFrame];
    [[[[self window] contentView] superview] addSubview:tabStripView_];
  } else {
    // No tabstrip so remove the tabContentArea offset.
    NSRect tabFrame = [tabContentArea_ frame];
    NSRect contentFrame = [[[self window] contentView] frame];
    tabFrame.size.height = contentFrame.size.height;
    [tabContentArea_ setFrame:tabFrame];
  }
}

- (void)removeOverlay {
  [self setUseOverlay:NO];
}

- (void)removeOverlayAfterDelay:(NSTimeInterval)delay {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(removeOverlay)
                                             object:nil];
  [self performSelector:@selector(removeOverlay)
             withObject:nil
             afterDelay:delay];
}

- (void)showOverlay {
  [self setUseOverlay:YES];
}

- (NSArray*)viewsToMoveToOverlay {
  return [NSArray arrayWithObjects:[self tabStripView],
            [self tabContentArea], nil];
}

// if |useOverlay| is true, we're moving views into the overlay's content
// area. If false, we're moving out of the overlay back into the window's
// content.
- (void)moveViewsBetweenWindowAndOverlay:(BOOL)useOverlay {
  NSView* moveTo = useOverlay ?
      [overlayWindow_ contentView] : [cachedContentView_ superview];
  NSArray* viewsToMove = [self viewsToMoveToOverlay];
  for (NSView* view in viewsToMove)
    [moveTo addSubview:view];
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
  if (useOverlay && !overlayWindow_) {
    DCHECK(!cachedContentView_);
    overlayWindow_ = [[NSPanel alloc] initWithContentRect:[[self window] frame]
                                                styleMask:NSBorderlessWindowMask
                                                  backing:NSBackingStoreBuffered
                                                    defer:YES];
    [overlayWindow_ setTitle:@"overlay"];
    [overlayWindow_ setBackgroundColor:[NSColor clearColor]];
    [overlayWindow_ setOpaque:NO];
    [overlayWindow_ setDelegate:self];
    NSView *contentView = [overlayWindow_ contentView];
    [contentView addSubview:[self tabStripView]];
    cachedContentView_ = [[self window] contentView];
    [self moveViewsBetweenWindowAndOverlay:useOverlay];
    [[self window] addChildWindow:overlayWindow_ ordered:NSWindowAbove];
    [overlayWindow_ orderFront:nil];
  } else if (!useOverlay && overlayWindow_) {
    DCHECK(cachedContentView_);
    [[self window] setContentView:cachedContentView_];
    [self moveViewsBetweenWindowAndOverlay:useOverlay];
    [[self window] makeFirstResponder:cachedContentView_];
    [[self window] display];
    [[self window] removeChildWindow:overlayWindow_];
    [overlayWindow_ orderOut:nil];
    [overlayWindow_ release];
    overlayWindow_ = nil;
    cachedContentView_ = nil;
  } else {
    NOTREACHED();
  }
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (BOOL)canReceiveFrom:(TabWindowController*)source {
  // subclass must implement
  NOTIMPLEMENTED();
  return NO;
}

- (void)moveTabView:(NSView*)view
     fromController:(TabWindowController*)dragController {
  NOTIMPLEMENTED();
}

- (NSView *)selectedTabView {
  NOTIMPLEMENTED();
  return nil;
}

- (void)layoutTabs {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // subclass must implement
  NOTIMPLEMENTED();
  return NULL;
}

- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                  yStretchiness:(CGFloat)yStretchiness {
  [self showNewTabButton:NO];
}

- (void)removePlaceholder {
  [self showNewTabButton:YES];
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

- (NSString*)selectedTabTitle {
  // subclass must implement
  NOTIMPLEMENTED();
  return @"";
}

- (BOOL)isNormalWindow {
  // subclass must implement
  NOTIMPLEMENTED();
  return YES;
}

- (BOOL)isTabDraggable:(NSView*)tabView {
  return ![lockedTabs_ containsObject:tabView];
}

- (void)setTab:(NSView*)tabView isDraggable:(BOOL)draggable {
  if (draggable)
    [lockedTabs_ removeObject:tabView];
  else
    [lockedTabs_ addObject:tabView];
}

@end
