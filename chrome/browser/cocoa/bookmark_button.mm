// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"

// The opacity of the bookmark button drag image.
static const CGFloat kDragImageOpacity = 0.7;

@interface BookmarkButton(Private)

// Make a drag image for the button.
- (NSImage*)dragImage;

@end  // @interface BookmarkButton(Private)

@implementation BookmarkButton

@synthesize delegate = delegate_;

// By default, NSButton ignores middle-clicks.
- (void)otherMouseUp:(NSEvent*)event {
  [self performClick:self];
}

// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event {
  if (delegate_) {
    // Ask our delegate to fill the pasteboard for us.
    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];

    [delegate_ fillPasteboard:pboard forDragOfButton:self];

    // At the moment, moving bookmarks causes their buttons (like me!)
    // to be destroyed and rebuilt.  Make sure we don't go away while on
    // the stack.
    [self retain];

    CGFloat yAt = [self bounds].size.height;
    NSSize dragOffset = NSMakeSize(0.0, 0.0);
    [self dragImage:[self dragImage] at:NSMakePoint(0, yAt) offset:dragOffset
              event:event pasteboard:pboard source:self slideBack:YES];

    // And we're done.	
    [self autorelease];
  } else {
    // Avoid blowing up, but we really shouldn't get here.
    NOTREACHED();
  }
}

- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)aPoint
           operation:(NSDragOperation)operation {
  [super endDrag];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return isLocal ? NSDragOperationCopy | NSDragOperationMove
                 : NSDragOperationCopy;
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
