// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"

NSString* kBookmarkButtonDragType = @"ChromiumBookmarkButtonDragType";

// http://codereview.chromium.org/180036/diff/3001/3004
namespace {
const CGFloat kWebDragStartHysteresisX = 5.0;
const CGFloat kWebDragStartHysteresisY = 5.0;
}

@implementation BookmarkButton

@synthesize draggable = draggable_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    draggable_ = YES;
  }
  return self;
}

// By default, NSButton ignores middle-clicks.
- (void)otherMouseUp:(NSEvent*)event {
  [self performClick:self];
}

- (void)beginDrag:(NSEvent*)event {
  NSSize dragOffset = NSMakeSize(0.0, 0.0);
  NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareTypes:[NSArray arrayWithObject:kBookmarkButtonDragType]
                 owner:self];

  // This NSData is no longer referenced once the function ends so
  // there is no need to retain/release when placing in here as an
  // opaque pointer.
  [pboard setData:[NSData dataWithBytes:&self length:sizeof(self)]
          forType:kBookmarkButtonDragType];

  // This won't work if the source view is clipped.  Fortunately, we
  // don't display clipped bookmark buttons.
  [[self superview] lockFocus];
  scoped_nsobject<NSBitmapImageRep>
      bitmapImage([[NSBitmapImageRep alloc]
                    initWithFocusedViewRect:[self frame]]);

  [[self superview] unlockFocus];
  scoped_nsobject<NSImage> imageDrag([[NSImage alloc]
                                       initWithSize:[bitmapImage size]]);
  [imageDrag addRepresentation:bitmapImage];

  // At the moment, moving bookmarks causes their buttons (like me!)
  // to be destroyed and rebuilt.  Make sure we don't go away while on
  // the stack.
  [self retain];

  CGFloat yAt = [self bounds].size.height;
  [self dragImage:imageDrag at:NSMakePoint(0, yAt) offset:dragOffset
            event:event pasteboard:pboard source:self slideBack:YES];

  // And we're done.
  [self autorelease];
}

- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)aPoint
           operation:(NSDragOperation)operation {
  beingDragged_ = NO;
  [[self cell] setHighlighted:NO];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return isLocal ? NSDragOperationMove : NSDragOperationNone;
}

- (void)mouseUp:(NSEvent*)theEvent {
  // This conditional is never true (DnD loops in Cocoa eat the mouse
  // up) but I added it in case future versions of Cocoa do unexpected
  // things.
  if (beingDragged_)
    return [super mouseUp:theEvent];

  // There are non-drag cases where a mouseUp: may happen
  // (e.g. mouse-down, cmd-tab to another application, move mouse,
  // mouse-up).  So we check.
  NSPoint viewLocal = [self convertPoint:[theEvent locationInWindow]
                                fromView:[[self window] contentView]];
  if (NSPointInRect(viewLocal, [self bounds])) {
    [self performClick:self];
  } else {
    [[self cell] setHighlighted:NO];
  }
}

// Mimic "begin a click" operation visually.  Do NOT follow through
// with normal button event handling.
- (void)mouseDown:(NSEvent*)theEvent {
  [[self cell] setHighlighted:YES];
  initialMouseDownLocation_ = [theEvent locationInWindow];
}

// Return YES if we have crossed a threshold of movement after
// mouse-down when we should begin a drag.  Else NO.
- (BOOL)hasCrossedDragThreshold:(NSEvent*)theEvent {
  NSPoint currentLocation = [theEvent locationInWindow];
  if ((abs(currentLocation.x - initialMouseDownLocation_.x) >
       kWebDragStartHysteresisX) ||
      (abs(currentLocation.y - initialMouseDownLocation_.y) >
       kWebDragStartHysteresisY)) {
    return YES;
  } else {
    return NO;
  }
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (beingDragged_)
    [super mouseDragged:theEvent];
  else {
    if (draggable_ && [self hasCrossedDragThreshold:theEvent]) {
      [self beginDrag:theEvent];
    }
  }
}

@end
