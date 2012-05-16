// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/closure_blocks_leopard_compat.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string_util.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_model_observer_bridge.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@interface BaseBubbleController (Private)
- (void)updateOriginFromAnchor;
- (void)activateTabWithContents:(TabContentsWrapper*)newContents
               previousContents:(TabContentsWrapper*)oldContents
                        atIndex:(NSInteger)index
                    userGesture:(bool)wasUserGesture;
@end

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
typedef unsigned long long NSEventMask;

@interface NSEvent (SnowLeopardDeclarations)
+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask
                                   handler:(NSEvent* (^)(NSEvent*))block;
+ (void)removeMonitor:(id)eventMonitor;
@end

@interface NSOperationQueue (SnowLeopardDeclarations)
+ (id)mainQueue;
@end

@interface NSNotificationCenter (SnowLeopardDeclarations)
- (id)addObserverForName:(NSString*)name
                  object:(id)obj
                   queue:(NSOperationQueue*)queue
              usingBlock:(void (^)(NSNotification*))block;
@end
#endif  // MAC_OS_X_VERSION_10_6

@implementation BaseBubbleController

@synthesize parentWindow = parentWindow_;
@synthesize anchorPoint = anchor_;
@synthesize bubble = bubble_;

- (id)initWithWindowNibPath:(NSString*)nibPath
               parentWindow:(NSWindow*)parentWindow
                 anchoredAt:(NSPoint)anchoredAt {
  nibPath = [base::mac::FrameworkBundle() pathForResource:nibPath
                                                   ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    anchor_ = anchoredAt;

    // Watch to see if the parent window closes, and if so, close this one.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:parentWindow_];
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

    DCHECK(![[self window] delegate]);
    [theWindow setDelegate:self];

    scoped_nsobject<InfoBubbleView> contentView(
        [[InfoBubbleView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)]);
    [theWindow setContentView:contentView.get()];
    bubble_ = contentView.get();

    // Watch to see if the parent window closes, and if so, close this one.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:parentWindow_];

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

- (void)setAnchorPoint:(NSPoint)anchor {
  anchor_ = anchor;
  [self updateOriginFromAnchor];
}

- (NSBox*)separatorWithFrame:(NSRect)frame {
  frame.size.height = 1.0;
  scoped_nsobject<NSBox> spacer([[NSBox alloc] initWithFrame:frame]);
  [spacer setBoxType:NSBoxSeparator];
  [spacer setBorderType:NSLineBorder];
  [spacer setAlphaValue:0.2];
  return [spacer.release() autorelease];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
  parentWindow_ = nil;
  [self close];
}

- (void)windowWillClose:(NSNotification*)notification {
  // We caught a close so we don't need to watch for the parent closing.
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
  [window makeKeyAndOrderFront:self];
  [self registerKeyStateEventTap];
}

- (void)close {
  // The bubble will be closing, so remove the event taps.
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

  [[[self window] parentWindow] removeChildWindow:[self window]];
  [super close];
}

// The controller is the delegate of the window so it receives did resign key
// notifications. When key is resigned mirror Windows behavior and close the
// window.
- (void)windowDidResignKey:(NSNotification*)notification {
  NSWindow* window = [self window];
  DCHECK_EQ([notification object], window);
  if ([window isVisible]) {
    // If the window isn't visible, it is already closed, and this notification
    // has been sent as part of the closing operation, so no need to close.
    [self close];
  }
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
      addLocalMonitorForEventsMatchingMask:NSLeftMouseDownMask
      handler:^NSEvent* (NSEvent* event) {
          if (event.window != window) {
            // Call via the runloop because this block is called in the
            // middle of event dispatch.
            [self performSelector:@selector(windowDidResignKey:)
                       withObject:note
                       afterDelay:0];
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
      if ([bubble_ arrowLocation] == info_bubble::kTopRight) {
        origin.x -= NSWidth([window frame]) - offsets.width;
      } else {
        origin.x -= offsets.width;
      }
      break;
    }

    case info_bubble::kAlignEdgeToAnchorEdge:
      // If the arrow is to the right then move the origin so that the right
      // edge aligns with the anchor. If the arrow is to the left then there's
      // nothing to do becaues the left edge is already aligned with the left
      // edge of the anchor.
      if ([bubble_ arrowLocation] == info_bubble::kTopRight) {
        origin.x -= NSWidth([window frame]);
      }
      break;

    default:
      NOTREACHED();
  }

  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];
}

- (void)activateTabWithContents:(TabContentsWrapper*)newContents
               previousContents:(TabContentsWrapper*)oldContents
                        atIndex:(NSInteger)index
                    userGesture:(bool)wasUserGesture {
  // The user switched tabs; close.
  [self close];
}

@end  // BaseBubbleController
