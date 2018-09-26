// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"

#import <objc/runtime.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/tabbed_browser_window.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"

// As of macOS 10.13 NSWindow lifetimes after closing are unpredictable. Chrome
// frees resources on window close so this new behavior created problems such as
// browser tests failing to complete (see https://crbug.com/749196 ). To work
// around this new behavior TabWindowController no longer acts as the NSWindow's
// NSWindowController or NSWindowDelegate but instead uses a
// TabWindowControllerProxy instance. The TabWindowController and its subclasses
// still expect to receive NSWindowDelegate messages, which the
// TabWindowControllerProxy forwards along.
@interface TabWindowControllerProxy : NSWindowController<NSWindowDelegate>
@property(assign, nonatomic) TabWindowController* tabWindowController;
@end

@implementation TabWindowControllerProxy

@synthesize tabWindowController = tabWindowController_;

- (void)dealloc {
  // The TabWindowControllerProxy should outlive the TabWindowController.
  DCHECK(!tabWindowController_);
  [super dealloc];
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  if ([super respondsToSelector:aSelector]) {
    return YES;
  }
  return [self.tabWindowController respondsToSelector:aSelector];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)aSelector {
  NSMethodSignature* signature = [super methodSignatureForSelector:aSelector];

  return signature
             ? signature
             : [self.tabWindowController methodSignatureForSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation*)anInvocation {
  [anInvocation setTarget:self.tabWindowController];
  [anInvocation invoke];
}

@end

@interface TabWindowController () {
  base::scoped_nsobject<TabWindowControllerProxy> nsWindowController_;
}

- (void)setUseOverlay:(BOOL)useOverlay;

// The tab strip background view should always be inserted as the back-most
// subview of the contentView.
- (void)insertTabStripBackgroundViewIntoWindow:(NSWindow*)window
                                      titleBar:(BOOL)hasTitleBar;

// Called when NSWindowWillEnterFullScreenNotification notification received.
// Makes visual effects view hidden as it should not be displayed in fullscreen.
- (void)windowWillEnterFullScreenNotification:(NSNotification*)notification;

// Called when NSWindowWillExitFullScreenNotification notification received.
// Makes visual effects view visible since it was hidden in fullscreen.
- (void)windowWillExitFullScreenNotification:(NSNotification*)notification;

@end

@interface TabWindowOverlayWindow : NSWindow
@end

@implementation TabWindowOverlayWindow

- (const ui::ThemeProvider*)themeProvider {
  return [[self parentWindow] themeProvider];
}

- (ThemedWindowStyle)themedWindowStyle {
  return [[self parentWindow] themedWindowStyle];
}

- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment {
  return [[self parentWindow] themeImagePositionForAlignment:alignment];
}

- (BOOL)hasDarkTheme {
  return [[self parentWindow] hasDarkTheme];
}

- (BOOL)inIncognitoModeWithSystemTheme {
  return [[self parentWindow] inIncognitoModeWithSystemTheme];
}

@end

// Subview of the window's contentView, contains everything but the tab strip.
@interface ChromeContentView : NSView
@end

@implementation ChromeContentView

// NSView overrides.

// Since Auto Layout and frame-based layout behave differently in small but
// important ways (e.g. Auto Layout can restrict window resizing, frame-based
// layout doesn't log a warning when a view's autoresizing mask can't be
// maintained), ensure that it's on instead of letting it depend on content.
+ (BOOL)requiresConstraintBasedLayout {
  // TODO(sdy): Turn back on (or remove) after investigating a performance
  // regression: https://crbug.com/706931
  return NO;
}

@end

@implementation TabWindowController

+ (TabWindowController*)tabWindowControllerForWindow:(NSWindow*)window {
  while (window) {
    TabWindowControllerProxy* nsWindowController =
        base::mac::ObjCCast<TabWindowControllerProxy>(
            [window windowController]);

    if (nsWindowController) {
      return [nsWindowController tabWindowController];
    }
    window = [window parentWindow];
  }
  return nil;
}

- (id)initTabWindowControllerWithTabStrip:(BOOL)hasTabStrip
                                 titleBar:(BOOL)hasTitleBar {
  const CGFloat kDefaultWidth = WindowSizer::kWindowMaxDefaultWidth;
  const CGFloat kDefaultHeight = 600;

  if (self = [self init]) {
    NSRect contentRect = NSMakeRect(60, 229, kDefaultWidth, kDefaultHeight);
    base::scoped_nsobject<FramedBrowserWindow> window(
        [(hasTabStrip
              ? [TabbedBrowserWindow alloc]
              : [FramedBrowserWindow alloc]) initWithContentRect:contentRect]);
    [window setReleasedWhenClosed:YES];
    [window setAutorecalculatesKeyViewLoop:YES];

    nsWindowController_.reset(
        [[TabWindowControllerProxy alloc] initWithWindow:window]);
    [nsWindowController_ setTabWindowController:self];

    [[self window] setDelegate:nsWindowController_];

    // Insert ourselves into the repsonder chain. First, find the responder
    // that comes before nsWindowController_.
    NSResponder* nextResponderToCheck = [self window];
    while ([nextResponderToCheck nextResponder] &&
           [nextResponderToCheck nextResponder] != nsWindowController_.get()) {
      nextResponderToCheck = [nextResponderToCheck nextResponder];
    }
    // If nextResponder is nil, nsWindowController_ is not in the responder
    // chain.
    DCHECK([nextResponderToCheck nextResponder]);
    // Insert before nsWindowController_.
    [nextResponderToCheck setNextResponder:self];
    [self setNextResponder:nsWindowController_];

    chromeContentView_.reset([[ChromeContentView alloc]
        initWithFrame:NSMakeRect(0, 0, kDefaultWidth, kDefaultHeight)]);
    [chromeContentView_
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [chromeContentView_ setWantsLayer:YES];
    [[[self window] contentView] addSubview:chromeContentView_];

    tabContentArea_.reset(
        [[FastResizeView alloc] initWithFrame:[chromeContentView_ bounds]]);
    [tabContentArea_
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [chromeContentView_ addSubview:tabContentArea_];

    [self insertTabStripBackgroundViewIntoWindow:window titleBar:hasTitleBar];

    // |windowWillEnterFullScreen:| and |windowWillExitFullScreen:| are
    // already called because self is a delegate for the window. However this
    // class is designed for subclassing and can not implement
    // NSWindowDelegate methods (because subclasses can do so as well and they
    // should be able to). TODO(crbug.com/654656): Move |visualEffectView_| to
    // subclass.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowWillEnterFullScreenNotification:)
               name:NSWindowWillEnterFullScreenNotification
             object:window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowWillExitFullScreenNotification:)
               name:NSWindowWillExitFullScreenNotification
             object:window];
  }

  return self;
}

- (void)dealloc {
  // Restore the reponder chain. Find the responder before us in the chain.
  NSResponder* nextResponderToCheck = [self window];
  while ([nextResponderToCheck nextResponder] &&
         [nextResponderToCheck nextResponder] != self) {
    nextResponderToCheck = [nextResponderToCheck nextResponder];
  }
  [nextResponderToCheck setNextResponder:[self nextResponder]];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[self window] setDelegate:nil];
  [nsWindowController_ setTabWindowController:nil];
  [nsWindowController_ setWindow:nil];
  [super dealloc];
}

- (NSWindow*)window {
  return [nsWindowController_ window];
}

- (void)setWindow:(NSWindow*)aWindow {
  [nsWindowController_ setWindow:aWindow];
}

- (NSWindowController*)nsWindowController {
  return nsWindowController_;
}

- (NSVisualEffectView*)visualEffectView {
  return nil;
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
}

- (NSWindow*)overlayWindow {
  return nil;
}

- (BOOL)shouldConstrainFrameRect {
  // If we currently have an overlay window, do not attempt to change the
  // window's size, as our overlay window doesn't know how to resize properly.
  return NO;
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

- (void)detachedWindowEnterFullscreenIfNeeded:(TabWindowController*)source {
  // Subclasses should implement this.
  NOTIMPLEMENTED();
}

- (void)insertPlaceholderForTab:(TabViewCocoa*)tab frame:(NSRect)frame {
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

- (BOOL)isTabFullyVisible:(TabViewCocoa*)tab {
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

- (CGFloat)menubarOffset {
  // Subclasses should implement this.
  NOTIMPLEMENTED();
  return 0;
}

- (CGFloat)menubarHeight {
  // The height of the menubar. We can't use |-[NSMenu menuBarHeight]| since it
  // returns 0 when the menu bar is hidden.
  const CGFloat kMenubarHeight = 22;
  return kMenubarHeight;
}

- (BOOL)isInAnyFullscreenMode {
  // Subclass must implement.
  NOTIMPLEMENTED();
  return NO;
}

- (NSView*)avatarView {
  return nil;
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

- (void)insertTabStripBackgroundViewIntoWindow:(NSWindow*)window
                                      titleBar:(BOOL)hasTitleBar {
}

// Called when the size of the window content area has changed. Override to
// position specific views. Base class implementation does nothing.
- (void)layoutSubviews {
  NOTIMPLEMENTED();
}

- (void)windowWillEnterFullScreenNotification:(NSNotification*)notification {
}

- (void)windowWillExitFullScreenNotification:(NSNotification*)notification {
}

@end
