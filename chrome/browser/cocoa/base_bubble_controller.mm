// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/base_bubble_controller.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#include "grit/generated_resources.h"

@implementation BaseBubbleController

@synthesize parentWindow = parentWindow_;
@synthesize bubble = bubble_;

- (id)initWithWindowNibPath:(NSString*)nibPath
               parentWindow:(NSWindow*)parentWindow
                 anchoredAt:(NSPoint)anchoredAt {
  nibPath = [mac_util::MainAppBundle() pathForResource:nibPath
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

  [bubble_ setBubbleType:info_bubble::kWhiteInfoBubble];
  [bubble_ setArrowLocation:info_bubble::kTopRight];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
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
  NSWindow* window = [self window];  // completes nib load

  NSPoint origin = anchor_;
  NSSize offsets = NSMakeSize(info_bubble::kBubbleArrowXOffset +
                              info_bubble::kBubbleArrowWidth / 2.0, 0);
  offsets = [[parentWindow_ contentView] convertSize:offsets toView:nil];
  origin.x += offsets.width;
  if ([bubble_ arrowLocation] == info_bubble::kTopRight)
    origin.x -= NSWidth([window frame]);
  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];
  [parentWindow_ addChildWindow:window ordered:NSWindowAbove];
  [window makeKeyAndOrderFront:self];
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
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

// By implementing this, ESC causes the window to go away.
- (IBAction)cancel:(id)sender {
  // This is not a "real" cancel as potential changes to the radio group are not
  // undone. That's ok.
  [self close];
}
@end  // BaseBubbleController
