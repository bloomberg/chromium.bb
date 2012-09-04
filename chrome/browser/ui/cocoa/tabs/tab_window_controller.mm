// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/theme_provider.h"

@interface TabWindowController(PRIVATE)
- (void)setUseOverlay:(BOOL)useOverlay;
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

- (NSPoint)themePatternPhase {
  if ([self parentWindow])
    return [[[self parentWindow] windowController] themePatternPhase];
  return NSZeroPoint;
}

@end

@implementation TabWindowController
@synthesize tabContentArea = tabContentArea_;

// Add the top tab strop to the window, above the content box and add it to the
// view hierarchy as a sibling of the content view so it can overlap with the
// window frame.
- (void)addTabStripToWindow {
  NSRect contentFrame = [tabContentArea_ frame];
  NSRect tabFrame =
      NSMakeRect(0, NSMaxY(contentFrame),
                 NSWidth(contentFrame),
                 NSHeight([tabStripView_ frame]));
  [tabStripView_ setFrame:tabFrame];
  NSView* contentParent = [[[self window] contentView] superview];
  [contentParent addSubview:tabStripView_];
}

- (void)windowDidLoad {
  // Cache the difference in height between the window content area and the
  // tab content area.
  NSRect tabFrame = [tabContentArea_ frame];
  NSRect contentFrame = [[[self window] contentView] frame];
  contentAreaHeightDelta_ = NSHeight(contentFrame) - NSHeight(tabFrame);

  if ([self hasTabStrip]) {
    [self addTabStripToWindow];
  } else {
    // No top tabstrip so remove the tabContentArea offset.
    tabFrame.size.height = contentFrame.size.height;
    [tabContentArea_ setFrame:tabFrame];
  }
}

// Return the appropriate tab strip based on whether or not side tabs are
// enabled.
- (TabStripView*)tabStripView {
  return tabStripView_;
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

// if |useOverlay| is true, we're moving views into the overlay's content
// area. If false, we're moving out of the overlay back into the window's
// content.
- (void)moveViewsBetweenWindowAndOverlay:(BOOL)useOverlay {
  if (useOverlay) {
    [[[overlayWindow_ contentView] superview] addSubview:[self tabStripView]];
    // Add the original window's content view as a subview of the overlay
    // window's content view.  We cannot simply use setContentView: here because
    // the overlay window has a different content size (due to it being
    // borderless).
    [[overlayWindow_ contentView] addSubview:cachedContentView_];
  } else {
    [[self window] setContentView:cachedContentView_];
    // The TabStripView always needs to be in front of the window's content
    // view and therefore it should always be added after the content view is
    // set.
    [[[[self window] contentView] superview] addSubview:[self tabStripView]];
    [[[[self window] contentView] superview] updateTrackingAreas];
  }
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
    DCHECK(!cachedContentView_);
    overlayWindow_ = [[TabWindowOverlayWindow alloc]
                         initWithContentRect:[window frame]
                                   styleMask:NSBorderlessWindowMask
                                     backing:NSBackingStoreBuffered
                                       defer:YES];
    [overlayWindow_ setTitle:@"overlay"];
    [overlayWindow_ setBackgroundColor:[NSColor clearColor]];
    [overlayWindow_ setOpaque:NO];
    [overlayWindow_ setDelegate:self];
    cachedContentView_ = [window contentView];
    [window addChildWindow:overlayWindow_ ordered:NSWindowAbove];
    // Sets explictly nil to the responder and then restores it.
    // Leaving the first responder non-null here
    // causes [RenderWidgethostViewCocoa resignFirstResponder] and
    // following RenderWidgetHost::Blur(), which results unexpected
    // focus lost.
    focusBeforeOverlay_.reset([[FocusTracker alloc] initWithWindow:window]);
    [window makeFirstResponder:nil];
    [self moveViewsBetweenWindowAndOverlay:useOverlay];
    [overlayWindow_ orderFront:nil];
  } else if (!useOverlay && overlayWindow_) {
    DCHECK(cachedContentView_);
    [window setContentView:cachedContentView_];
    [self moveViewsBetweenWindowAndOverlay:useOverlay];
    [focusBeforeOverlay_ restoreFocusInWindow:window];
    focusBeforeOverlay_.reset(nil);
    [window display];
    [window removeChildWindow:overlayWindow_];
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

- (void)moveTabView:(NSView*)view
     fromController:(TabWindowController*)dragController {
  NOTIMPLEMENTED();
}

- (NSView*)activeTabView {
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

// Called when the size of the window content area has changed. Override to
// position specific views. Base class implementation does nothing.
- (void)layoutSubviews {
  NOTIMPLEMENTED();
}

@end
