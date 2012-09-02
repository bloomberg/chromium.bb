// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"

#include <map>

#include "base/logging.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_animation.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_info.h"

namespace {

// Maps parent windows to sheet controllers.
NSMutableDictionary* g_sheetControllers;

// Get a value for the given window that can be used as a key in a dictionary.
NSValue* GetKeyForParentWindow(NSWindow* parent_window) {
  return [NSValue valueWithNonretainedObject:parent_window];
}

}  // namespace

// An invisible overlay window placed on top of the sheet's parent view.
// This window blocks interaction with the underlying view.
@interface CWSheetOverlayWindow : NSWindow {
  scoped_nsobject<ConstrainedWindowSheetController> controller_;
}
@end

@interface ConstrainedWindowSheetController ()
- (id)initWithParentWindow:(NSWindow*)parentWindow;
- (ConstrainedWindowSheetInfo*)findSheetInfoForParentView:(NSView*)parentView;
- (ConstrainedWindowSheetInfo*)findSheetInfoForSheet:(NSWindow*)sheet;
- (void)onParentWindowWillClose:(NSNotification*)note;
- (void)onParentViewFrameDidChange:(NSNotification*)note;
- (void)updateSheetPosition:(NSView*)parentView;
- (NSRect)overlayWindowFrameForParentView:(NSView*)parentView;
- (NSPoint)originForSheet:(NSWindow*)sheet
          inContainerRect:(NSRect)containerRect;
- (void)onOveralyWindowMouseDown:(CWSheetOverlayWindow*)overlayWindow;
- (void)animationDidEnd:(NSAnimation*)animation;
- (void)closeSheetWithoutAnimation:(ConstrainedWindowSheetInfo*)info;
@end

@implementation CWSheetOverlayWindow

- (id)initWithContentRect:(NSRect)rect
               controller:(ConstrainedWindowSheetController*)controller {
  if ((self = [super initWithContentRect:rect
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIgnoresMouseEvents:NO];
    [self setReleasedWhenClosed:NO];
    controller_.reset([controller retain]);
  }
  return self;
}

- (void)mouseDown:(NSEvent*)event {
  [controller_ onOveralyWindowMouseDown:self];
}

@end

@implementation ConstrainedWindowSheetController

+ (ConstrainedWindowSheetController*)
    controllerForParentWindow:(NSWindow*)parentWindow {
  DCHECK(parentWindow);
  ConstrainedWindowSheetController* controller =
      [g_sheetControllers objectForKey:GetKeyForParentWindow(parentWindow)];
  if (controller)
    return controller;

  scoped_nsobject<ConstrainedWindowSheetController> new_controller(
      [[ConstrainedWindowSheetController alloc]
          initWithParentWindow:parentWindow]);
  if (!g_sheetControllers)
    g_sheetControllers = [[NSMutableDictionary alloc] init];
  [g_sheetControllers setObject:new_controller
                         forKey:GetKeyForParentWindow(parentWindow)];
  return new_controller;
}

+ (ConstrainedWindowSheetController*)controllerForSheet:(NSWindow*)sheet {
  for (ConstrainedWindowSheetController* controller in
       [g_sheetControllers objectEnumerator]) {
    if ([controller findSheetInfoForSheet:sheet])
      return controller;
  }
  return nil;
}

- (id)initWithParentWindow:(NSWindow*)parentWindow {
  if ((self = [super init])) {
    parentWindow_.reset([parentWindow retain]);
    sheets_.reset([[NSMutableArray alloc] init]);

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onParentWindowWillClose:)
               name:NSWindowWillCloseNotification
             object:parentWindow_];
  }
  return self;
}

- (void)showSheet:(NSWindow*)sheet
    forParentView:(NSView*)parentView {
  DCHECK(sheet);
  DCHECK(parentView);
  if (!activeView_.get())
    activeView_.reset([parentView retain]);

  NSRect rect = [self overlayWindowFrameForParentView:parentView];
  scoped_nsobject<NSWindow> overlayWindow(
      [[CWSheetOverlayWindow alloc] initWithContentRect:rect
                                             controller:self]);
  [sheet setFrameOrigin:[self originForSheet:sheet inContainerRect:rect]];

  scoped_nsobject<ConstrainedWindowSheetInfo> info(
      [[ConstrainedWindowSheetInfo alloc] initWithSheet:sheet
                                             parentView:parentView
                                          overlayWindow:overlayWindow]);
  [sheets_ addObject:info];
  if (![activeView_ isEqual:parentView]) {
    [info hideSheet];
  } else {
    scoped_nsobject<NSAnimation> animation(
        [[ConstrainedWindowAnimationShow alloc] initWithWindow:sheet]);
    [info setAnimation:animation];
    [animation startAnimation];
  }

  [parentWindow_ addChildWindow:overlayWindow
                        ordered:NSWindowAbove];
  [overlayWindow addChildWindow:sheet
                        ordered:NSWindowAbove];

  [parentView setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onParentViewFrameDidChange:)
             name:NSViewFrameDidChangeNotification
           object:parentView];
}

- (void)closeSheet:(NSWindow*)sheet {
  ConstrainedWindowSheetInfo* info = [self findSheetInfoForSheet:sheet];
  DCHECK(info);

  if (![activeView_ isEqual:[info parentView]]) {
    [self closeSheetWithoutAnimation:info];
    return;
  }

  scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationHide alloc] initWithWindow:sheet]);
  [animation setDelegate:self];
  [info setAnimation:animation];
  [animation startAnimation];
}


- (void)parentViewDidBecomeActive:(NSView*)parentView {
  [[self findSheetInfoForParentView:activeView_] hideSheet];
  activeView_.reset([parentView retain]);
  [[self findSheetInfoForParentView:activeView_] showSheet];
}

- (int)sheetCount {
  return [sheets_ count];
}

- (ConstrainedWindowSheetInfo*)findSheetInfoForParentView:(NSView*)parentView {
  for (ConstrainedWindowSheetInfo* info in sheets_.get()) {
    if ([parentView isEqual:[info parentView]])
      return info;
  }
  return NULL;
}

- (ConstrainedWindowSheetInfo*)findSheetInfoForSheet:(NSWindow*)sheet {
  for (ConstrainedWindowSheetInfo* info in sheets_.get()) {
    if ([sheet isEqual:[info sheet]])
      return info;
  }
  return NULL;
}

- (void)onParentWindowWillClose:(NSNotification*)note {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:parentWindow_];

  // Close all sheets.
  NSArray* sheets = [NSArray arrayWithArray:sheets_];
  for (ConstrainedWindowSheetInfo* info in sheets)
    [self closeSheetWithoutAnimation:info];

  // Delete this instance.
  [g_sheetControllers removeObjectForKey:GetKeyForParentWindow(parentWindow_)];
  if (![g_sheetControllers count]) {
    [g_sheetControllers release];
    g_sheetControllers = nil;
  }
}

- (void)onParentViewFrameDidChange:(NSNotification*)note {
  [self updateSheetPosition:[note object]];
}

- (void)updateSheetPosition:(NSView*)parentView {
  if (![activeView_ isEqual:parentView])
    return;
  ConstrainedWindowSheetInfo* info =
      [self findSheetInfoForParentView:parentView];
  DCHECK(info);
  NSRect rect = [self overlayWindowFrameForParentView:parentView];
  [[info overlayWindow] setFrame:rect display:YES];
  [[info sheet] setFrameOrigin:[self originForSheet:[info sheet]
                                    inContainerRect:rect]];
}

- (NSRect)overlayWindowFrameForParentView:(NSView*)parentView {
  NSRect viewFrame = [parentView frame];
  viewFrame = [[parentView superview] convertRect:viewFrame toView:nil];
  viewFrame.origin = [[parentView window] convertBaseToScreen:viewFrame.origin];
  return viewFrame;
}

- (NSPoint)originForSheet:(NSWindow*)sheet
          inContainerRect:(NSRect)containerRect {
  NSSize sheetSize = [sheet frame].size;
  NSPoint origin;
  origin.x =
      NSMinX(containerRect) + (NSWidth(containerRect) - sheetSize.width) / 2.0;
  origin.y = NSMaxY(containerRect) + 5 - sheetSize.height;
  return origin;
}

- (void)onOveralyWindowMouseDown:(CWSheetOverlayWindow*)overlayWindow {
  ConstrainedWindowSheetInfo* info = nil;
  for (ConstrainedWindowSheetInfo* curInfo in sheets_.get()) {
    if ([overlayWindow isEqual:[curInfo overlayWindow]]) {
      info = curInfo;
      break;
    }
  }
  DCHECK(info);
  if ([[info animation] isAnimating])
    return;

  scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationPulse alloc] initWithWindow:[info sheet]]);
  [info setAnimation:animation];
  [animation startAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  ConstrainedWindowSheetInfo* info = nil;
  for (ConstrainedWindowSheetInfo* curInfo in sheets_.get()) {
    if ([animation isEqual:[curInfo animation]]) {
      info = curInfo;
      break;
    }
  }
  DCHECK(info);

  // To avoid reentrancy close the sheet in the next event cycle.
  [self performSelector:@selector(closeSheetWithoutAnimation:)
             withObject:info
             afterDelay:0];
}

- (void)closeSheetWithoutAnimation:(ConstrainedWindowSheetInfo*)info {
  if (![sheets_ containsObject:info])
    return;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSViewFrameDidChangeNotification
              object:[info parentView]];

  [[info animation] stopAnimation];
  [parentWindow_ removeChildWindow:[info overlayWindow]];
  [[info overlayWindow] removeChildWindow:[info sheet]];
  [[info sheet] close];
  [[info overlayWindow] close];
  [sheets_ removeObject:info];
}

@end

@implementation ConstrainedWindowSheetController (TestingAPI)

- (void)endAnimationForSheet:(NSWindow*)sheet {
  [[[self findSheetInfoForSheet:sheet] animation] stopAnimation];
}

@end
