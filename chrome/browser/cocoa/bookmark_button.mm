// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

NSString* kBookmarkButtonDragType = @"ChromiumBookmarkButtonDragType";

namespace {

// Code taken from <http://codereview.chromium.org/180036/diff/3001/3004>.
// TODO(viettrungluu): Do we want common, standard code for drag hysteresis?
const CGFloat kWebDragStartHysteresisX = 5.0;
const CGFloat kWebDragStartHysteresisY = 5.0;

// The opacity of the bookmark button drag image.
const CGFloat kDragImageOpacity = 0.8;

}

@interface BookmarkButton(Private)

// Make a drag image for the button.
- (NSImage*)dragImage;

@end  // @interface BookmarkButton(Private)

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

  // At the moment, moving bookmarks causes their buttons (like me!)
  // to be destroyed and rebuilt.  Make sure we don't go away while on
  // the stack.
  [self retain];

  CGFloat yAt = [self bounds].size.height;
  [self dragImage:[self dragImage] at:NSMakePoint(0, yAt) offset:dragOffset
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

@implementation BookmarkButton(Private)

- (NSImage*)dragImage {
  NSRect bounds = [self bounds];

  // Grab the image from the screen and put it in an |NSImage|. We can't use
  // this directly since we need to clip it and set its opacity. This won't work
  // if the source view is clipped. Fortunately, we don't display clipped
  // bookmark buttons.
  [self lockFocus];
  scoped_nsobject<NSBitmapImageRep>
      bitmap([[NSBitmapImageRep alloc] initWithFocusedViewRect:bounds]);
  [self unlockFocus];
  scoped_nsobject<NSImage> image([[NSImage alloc] initWithSize:[bitmap size]]);
  [image addRepresentation:bitmap];

  // Make an autoreleased |NSImage|, which will be returned, and draw into it.
  // By default, the |NSImage| will be completely transparent.
  NSImage* dragImage =
      [[[NSImage alloc] initWithSize:[bitmap size]] autorelease];
  [dragImage lockFocus];

  // Draw the image with the appropriate opacity, clipping it tightly.
  GradientButtonCell* cell = static_cast<GradientButtonCell*>([self cell]);
  DCHECK([cell isKindOfClass:[GradientButtonCell class]]);
  [[cell clipPathForFrame:bounds inView:self] setClip];
  [image drawAtPoint:NSMakePoint(0, 0)
            fromRect:NSMakeRect(0, 0, NSWidth(bounds), NSHeight(bounds))
           operation:NSCompositeSourceOver
            fraction:kDragImageOpacity];

  [dragImage unlockFocus];
  return dragImage;
}

@end  // @implementation BookmarkButton(Private)
