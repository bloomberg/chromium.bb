// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/string_util.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_model_observer_bridge.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@interface BaseBubbleController (Private)
- (void)registerForNotifications;
- (void)updateOriginFromAnchor;
- (void)activateTabWithContents:(content::WebContents*)newContents
               previousContents:(content::WebContents*)oldContents
                        atIndex:(NSInteger)index
                         reason:(int)reason;
- (void)recordAnchorOffset;
- (void)parentWindowDidResize:(NSNotification*)notification;
- (void)parentWindowWillClose:(NSNotification*)notification;
- (void)parentWindowWillBecomeFullScreen:(NSNotification*)notification;
- (void)closeCleanup;
@end

@implementation BaseBubbleController

@synthesize parentWindow = parentWindow_;
@synthesize anchorPoint = anchor_;
@synthesize bubble = bubble_;
@synthesize shouldOpenAsKeyWindow = shouldOpenAsKeyWindow_;
@synthesize shouldCloseOnResignKey = shouldCloseOnResignKey_;

- (id)initWithWindowNibPath:(NSString*)nibPath
               parentWindow:(NSWindow*)parentWindow
                 anchoredAt:(NSPoint)anchoredAt {
  nibPath = [base::mac::FrameworkBundle() pathForResource:nibPath
                                                   ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    anchor_ = anchoredAt;
    shouldOpenAsKeyWindow_ = YES;
    shouldCloseOnResignKey_ = YES;
    [self registerForNotifications];
  }
  return self;
}

- (id)initWithWindowNibPath:(NSString*)nibPath
             relativeToView:(NSView*)view
                     offset:(NSPoint)offset {
  DCHECK([view window]);
  NSWindow* window = [view window];
  NSRect bounds = [view convertRect:[view bounds] toView:nil];
  NSPoint anchor = NSMakePoint(NSMinX(bounds) + offset.x,
                               NSMinY(bounds) + offset.y);
  anchor = [window convertBaseToScreen:anchor];
  return [self initWithWindowNibPath:nibPath
                        parentWindow:window
                          anchoredAt:anchor];
}

- (id)initWithWindow:(NSWindow*)theWindow
        parentWindow:(NSWindow*)parentWindow
          anchoredAt:(NSPoint)anchoredAt {
  DCHECK(theWindow);
  if ((self = [super initWithWindow:theWindow])) {
    parentWindow_ = parentWindow;
    anchor_ = anchoredAt;
    shouldOpenAsKeyWindow_ = YES;
    shouldCloseOnResignKey_ = YES;

    DCHECK(![[self window] delegate]);
    [theWindow setDelegate:self];

    base::scoped_nsobject<InfoBubbleView> contentView(
        [[InfoBubbleView alloc] initWithFrame:NSZeroRect]);
    [theWindow setContentView:contentView.get()];
    bubble_ = contentView.get();

    [self registerForNotifications];
    [self awakeFromNib];
  }
  return self;
}

- (void)awakeFromNib {
  // Check all connections have been made in Interface Builder.
  DCHECK([self window]);
  DCHECK(bubble_);
  DCHECK_EQ(self, [[self window] delegate]);

  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:parentWindow_];
  if (bwc) {
    TabStripController* tabStripController = [bwc tabStripController];
    TabStripModel* tabStripModel = [tabStripController tabStripModel];
    tabStripObserverBridge_.reset(new TabStripModelObserverBridge(tabStripModel,
                                                                  self));
  }

  [bubble_ setArrowLocation:info_bubble::kTopRight];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)registerForNotifications {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  // Watch to see if the parent window closes, and if so, close this one.
  [center addObserver:self
             selector:@selector(parentWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:parentWindow_];
  // Watch for the full screen event, if so, close the bubble
  [center addObserver:self
             selector:@selector(parentWindowWillBecomeFullScreen:)
                 name:NSWindowWillEnterFullScreenNotification
               object:parentWindow_];
  // Watch for parent window's resizing, to ensure this one is always
  // anchored correctly.
  [center addObserver:self
             selector:@selector(parentWindowDidResize:)
                 name:NSWindowDidResizeNotification
               object:parentWindow_];
}

- (void)setAnchorPoint:(NSPoint)anchor {
  anchor_ = anchor;
  [self updateOriginFromAnchor];
}

- (void)recordAnchorOffset {
  // The offset of the anchor from the parent's upper-left-hand corner is kept
  // to ensure the bubble stays anchored correctly if the parent is resized.
  anchorOffset_ = NSMakePoint(NSMinX([parentWindow_ frame]),
                              NSMaxY([parentWindow_ frame]));
  anchorOffset_.x -= anchor_.x;
  anchorOffset_.y -= anchor_.y;
}

- (NSBox*)horizontalSeparatorWithFrame:(NSRect)frame {
  frame.size.height = 1.0;
  base::scoped_nsobject<NSBox> spacer([[NSBox alloc] initWithFrame:frame]);
  [spacer setBoxType:NSBoxSeparator];
  [spacer setBorderType:NSLineBorder];
  [spacer setAlphaValue:0.2];
  return [spacer.release() autorelease];
}

- (NSBox*)verticalSeparatorWithFrame:(NSRect)frame {
  frame.size.width = 1.0;
  base::scoped_nsobject<NSBox> spacer([[NSBox alloc] initWithFrame:frame]);
  [spacer setBoxType:NSBoxSeparator];
  [spacer setBorderType:NSLineBorder];
  [spacer setAlphaValue:0.2];
  return [spacer.release() autorelease];
}

- (void)parentWindowDidResize:(NSNotification*)notification {
  if (!parentWindow_)
    return;

  DCHECK_EQ(parentWindow_, [notification object]);
  NSPoint newOrigin = NSMakePoint(NSMinX([parentWindow_ frame]),
                                  NSMaxY([parentWindow_ frame]));
  newOrigin.x -= anchorOffset_.x;
  newOrigin.y -= anchorOffset_.y;
  [self setAnchorPoint:newOrigin];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
  parentWindow_ = nil;
  [self close];
}

- (void)parentWindowWillBecomeFullScreen:(NSNotification*)notification {
  parentWindow_ = nil;
  [self close];
}

- (void)closeCleanup {
  if (eventTap_) {
    [NSEvent removeMonitor:eventTap_];
    eventTap_ = nil;
  }
  if (resignationObserver_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:resignationObserver_
                  name:NSWindowDidResignKeyNotification
                object:nil];
    resignationObserver_ = nil;
  }

  tabStripObserverBridge_.reset();

  NSWindow* window = [self window];
  [[window parentWindow] removeChildWindow:window];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self closeCleanup];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self autorelease];
}

// We want this to be a child of a browser window.  addChildWindow:
// (called from this function) will bring the window on-screen;
// unfortunately, [NSWindowController showWindow:] will also bring it
// on-screen (but will cause unexpected changes to the window's
// position).  We cannot have an addChildWindow: and a subsequent
// showWindow:. Thus, we have our own version.
- (void)showWindow:(id)sender {
  NSWindow* window = [self window];  // Completes nib load.
  [self updateOriginFromAnchor];
  [parentWindow_ addChildWindow:window ordered:NSWindowAbove];
  if (shouldOpenAsKeyWindow_)
    [window makeKeyAndOrderFront:self];
  else
    [window orderFront:nil];
  [self registerKeyStateEventTap];
  [self recordAnchorOffset];
}

- (void)close {
  [self closeCleanup];
  [super close];
}

// The controller is the delegate of the window so it receives did resign key
// notifications. When key is resigned mirror Windows behavior and close the
// window.
- (void)windowDidResignKey:(NSNotification*)notification {
  NSWindow* window = [self window];
  DCHECK_EQ([notification object], window);
  if ([window isVisible] && [self shouldCloseOnResignKey]) {
    // If the window isn't visible, it is already closed, and this notification
    // has been sent as part of the closing operation, so no need to close.
    [self close];
  } else if ([window isVisible]) {
    // The bubble should not receive key events when it is no longer key window,
    // so disable sharing parent key state. Share parent key state is only used
    // to enable the close/minimize/maximize buttons of the parent window when
    // the bubble has key state, so disabling it here is safe.
    InfoBubbleWindow* bubbleWindow =
        base::mac::ObjCCastStrict<InfoBubbleWindow>([self window]);
    [bubbleWindow setAllowShareParentKeyState:NO];
  }
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  // Re-enable share parent key state to make sure the close/minimize/maximize
  // buttons of the parent window are active.
  InfoBubbleWindow* bubbleWindow =
      base::mac::ObjCCastStrict<InfoBubbleWindow>([self window]);
  [bubbleWindow setAllowShareParentKeyState:YES];
}

// Since the bubble shares first responder with its parent window, set
// event handlers to dismiss the bubble when it would normally lose key
// state.
- (void)registerKeyStateEventTap {
  // Parent key state sharing is only avaiable on 10.7+.
  if (!base::mac::IsOSLionOrLater())
    return;

  NSWindow* window = self.window;
  NSNotification* note =
      [NSNotification notificationWithName:NSWindowDidResignKeyNotification
                                    object:window];

  // The eventTap_ catches clicks within the application that are outside the
  // window.
  eventTap_ = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSLeftMouseDownMask |
                                           NSRightMouseDownMask
      handler:^NSEvent* (NSEvent* event) {
          if (event.window != window) {
            // Do it right now, because if this event is right mouse event,
            // it may pop up a menu. windowDidResignKey: will not run until
            // the menu is closed.
            if ([self respondsToSelector:@selector(windowDidResignKey:)]) {
              [self windowDidResignKey:note];
            }
          }
          return event;
      }];

  // The resignationObserver_ watches for when a window resigns key state,
  // meaning the key window has changed and the bubble should be dismissed.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  resignationObserver_ =
      [center addObserverForName:NSWindowDidResignKeyNotification
                          object:nil
                           queue:[NSOperationQueue mainQueue]
                      usingBlock:^(NSNotification* notif) {
                          [self windowDidResignKey:note];
                      }];
}

// By implementing this, ESC causes the window to go away.
- (IBAction)cancel:(id)sender {
  // This is not a "real" cancel as potential changes to the radio group are not
  // undone. That's ok.
  [self close];
}

// Takes the |anchor_| point and adjusts the window's origin accordingly.
- (void)updateOriginFromAnchor {
  NSWindow* window = [self window];
  NSPoint origin = anchor_;

  switch ([bubble_ alignment]) {
    case info_bubble::kAlignArrowToAnchor: {
      NSSize offsets = NSMakeSize(info_bubble::kBubbleArrowXOffset +
                                  info_bubble::kBubbleArrowWidth / 2.0, 0);
      offsets = [[parentWindow_ contentView] convertSize:offsets toView:nil];
      switch ([bubble_ arrowLocation]) {
        case info_bubble::kTopRight:
          origin.x -= NSWidth([window frame]) - offsets.width;
          break;
        case info_bubble::kTopLeft:
          origin.x -= offsets.width;
          break;
        case info_bubble::kTopCenter:
          origin.x -= NSWidth([window frame]) / 2.0;
          break;
        case info_bubble::kNoArrow:
          NOTREACHED();
          break;
      }
      break;
    }

    case info_bubble::kAlignEdgeToAnchorEdge:
      // If the arrow is to the right then move the origin so that the right
      // edge aligns with the anchor. If the arrow is to the left then there's
      // nothing to do because the left edge is already aligned with the left
      // edge of the anchor.
      if ([bubble_ arrowLocation] == info_bubble::kTopRight) {
        origin.x -= NSWidth([window frame]);
      }
      break;

    case info_bubble::kAlignRightEdgeToAnchorEdge:
      origin.x -= NSWidth([window frame]);
      break;

    case info_bubble::kAlignLeftEdgeToAnchorEdge:
      // Nothing to do.
      break;

    default:
      NOTREACHED();
  }

  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];
}

- (void)activateTabWithContents:(content::WebContents*)newContents
               previousContents:(content::WebContents*)oldContents
                        atIndex:(NSInteger)index
                         reason:(int)reason {
  // The user switched tabs; close.
  [self close];
}

@end  // BaseBubbleController
