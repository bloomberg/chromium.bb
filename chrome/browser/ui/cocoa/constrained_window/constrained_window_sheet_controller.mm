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
- (NSPoint)originForSheetSize:(NSSize)sheetSize
              inContainerRect:(NSRect)containerRect;
- (void)onOverlayWindowMouseDown:(CWSheetOverlayWindow*)overlayWindow;
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
  [controller_ onOverlayWindowMouseDown:self];
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
  [sheet setFrameOrigin:[self originForSheetSize:[sheet frame].size
                                 inContainerRect:rect]];

  scoped_nsobject<ConstrainedWindowSheetInfo> info(
      [[ConstrainedWindowSheetInfo alloc] initWithSheet:sheet
                                             parentView:parentView
                                          overlayWindow:overlayWindow]);
  [sheets_ addObject:info];
  BOOL showSheet = [activeView_ isEqual:parentView];
  scoped_nsobject<NSAnimation> animation;
  if (!showSheet) {
    [info hideSheet];
  } else {
    animation.reset(
        [[ConstrainedWindowAnimationShow alloc] initWithWindow:sheet]);
  }

  [parentWindow_ addChildWindow:overlayWindow
                        ordered:NSWindowAbove];
  [overlayWindow addChildWindow:sheet
                        ordered:NSWindowAbove];
  // Set focus to the sheet if the parent window is main. The parent window
  // may not have keyboard focus if it has a child window open, for example
  // a bubble.
  if (showSheet && [parentWindow_ isMainWindow])
    [sheet makeKeyAndOrderFront:nil];

  [parentView setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onParentViewFrameDidChange:)
             name:NSViewFrameDidChangeNotification
           object:parentView];

  [animation startAnimation];
}

- (NSPoint)originForSheet:(NSWindow*)sheet
           withWindowSize:(NSSize)size {
  ConstrainedWindowSheetInfo* info = [self findSheetInfoForSheet:sheet];
  DCHECK(info);
  NSRect containerRect =
      [self overlayWindowFrameForParentView:[info parentView]];
  return [self originForSheetSize:size inContainerRect:containerRect];
}

- (void)closeSheet:(NSWindow*)sheet {
  ConstrainedWindowSheetInfo* info = [self findSheetInfoForSheet:sheet];
  DCHECK(info);

  if ([activeView_ isEqual:[info parentView]]) {
    scoped_nsobject<NSAnimation> animation(
        [[ConstrainedWindowAnimationHide alloc] initWithWindow:sheet]);
    [animation startAnimation];
  }

  [self closeSheetWithoutAnimation:info];
}

- (void)parentViewDidBecomeActive:(NSView*)parentView {
  [[self findSheetInfoForParentView:activeView_] hideSheet];
  activeView_.reset([parentView retain]);
  [self updateSheetPosition:parentView];
  [[self findSheetInfoForParentView:activeView_] showSheet];
}

- (void)pulseSheet:(NSWindow*)sheet {
  ConstrainedWindowSheetInfo* info = [self findSheetInfoForSheet:sheet];
  DCHECK(info);
  if (![activeView_ isEqual:[info parentView]])
    return;

  scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationPulse alloc] initWithWindow:[info sheet]]);
  [animation startAnimation];
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
  NSView* parentView = [note object];
  if (![activeView_ isEqual:parentView])
    return;
  [self updateSheetPosition:parentView];
}

- (void)updateSheetPosition:(NSView*)parentView {
  ConstrainedWindowSheetInfo* info =
      [self findSheetInfoForParentView:parentView];
  if (!info)
    return;

  NSRect rect = [self overlayWindowFrameForParentView:parentView];
  [[info overlayWindow] setFrame:rect display:YES];
  NSPoint origin = [self originForSheetSize:[[info sheet] frame].size
                            inContainerRect:rect];
  [[info sheet] setFrameOrigin:origin];
}

- (NSRect)overlayWindowFrameForParentView:(NSView*)parentView {
  NSRect viewFrame = [parentView convertRect:[parentView bounds] toView:nil];
  viewFrame.origin = [[parentView window] convertBaseToScreen:viewFrame.origin];
  return viewFrame;
}

- (NSPoint)originForSheetSize:(NSSize)sheetSize
              inContainerRect:(NSRect)containerRect {
  NSPoint origin;
  origin.x = roundf(NSMinX(containerRect) +
                    (NSWidth(containerRect) - sheetSize.width) / 2.0);
  origin.y = NSMaxY(containerRect) + 5 - sheetSize.height;
  return origin;
}

- (void)onOverlayWindowMouseDown:(CWSheetOverlayWindow*)overlayWindow {
  for (ConstrainedWindowSheetInfo* curInfo in sheets_.get()) {
    if ([overlayWindow isEqual:[curInfo overlayWindow]]) {
      [self pulseSheet:[curInfo sheet]];
      [[curInfo sheet] makeKeyAndOrderFront:nil];
      break;
    }
  }
}

- (void)closeSheetWithoutAnimation:(ConstrainedWindowSheetInfo*)info {
  if (![sheets_ containsObject:info])
    return;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSViewFrameDidChangeNotification
              object:[info parentView]];

  [parentWindow_ removeChildWindow:[info overlayWindow]];
  [[info overlayWindow] removeChildWindow:[info sheet]];
  [[info sheet] close];
  [[info overlayWindow] close];
  [sheets_ removeObject:info];
}

@end
