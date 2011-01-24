// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/web_contents_drag_source.h"

#include "app/mac/nsimage_cache.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"

namespace {

// Make a drag image from the drop data.
// TODO(feldstein): Make this work
NSImage* MakeDragImage() {
  // TODO(feldstein): Just a stub for now. Make it do something (see, e.g.,
  // WebKit/WebKit/mac/Misc/WebNSViewExtras.m: |-_web_DragImageForElement:...|).

  // Default to returning a generic image.
  return app::mac::GetCachedImageWithName(@"nav.pdf");
}

// Flips screen and point coordinates over the y axis to work with webkit
// coordinate systems.
void FlipPointCoordinates(NSPoint& screenPoint,
                          NSPoint& localPoint,
                          NSView* view) {
  NSRect viewFrame = [view frame];
  localPoint.y = NSHeight(viewFrame) - localPoint.y;
  // Flip |screenPoint|.
  NSRect screenFrame = [[[view window] screen] frame];
  screenPoint.y = NSHeight(screenFrame) - screenPoint.y;
}

}  // namespace


@implementation WebContentsDragSource

- (id)initWithContentsView:(TabContentsViewCocoa*)contentsView
                pasteboard:(NSPasteboard*)pboard
         dragOperationMask:(NSDragOperation)dragOperationMask {
  if ((self = [super init])) {
    contentsView_ = contentsView;
    DCHECK(contentsView_);

    pasteboard_.reset([pboard retain]);
    DCHECK(pasteboard_.get());

    dragOperationMask_ = dragOperationMask;
  }

  return self;
}

- (NSImage*)dragImage {
  return MakeDragImage();
}

- (void)fillPasteboard {
  NOTIMPLEMENTED() << "Subclasses should implement fillPasteboard";
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return dragOperationMask_;
}

- (void)startDrag {
  [self fillPasteboard];
  NSEvent* currentEvent = [NSApp currentEvent];

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [contentsView_ window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];
  [window dragImage:[self dragImage]
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:pasteboard_
             source:self
          slideBack:YES];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint
  operation:(NSDragOperation)operation {
}

- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation {
  RenderViewHost* rvh = [contentsView_ tabContents]->render_view_host();
  if (rvh) {
    rvh->DragSourceSystemDragEnded();

    NSPoint localPoint = [contentsView_ convertPoint:screenPoint fromView: nil];
    FlipPointCoordinates(screenPoint, localPoint, contentsView_);
    rvh->DragSourceEndedAt(localPoint.x, localPoint.y,
                           screenPoint.x, screenPoint.y,
                           static_cast<WebKit::WebDragOperation>(operation));
  }

  // Make sure the pasteboard owner isn't us.
  [pasteboard_ declareTypes:[NSArray array] owner:nil];
}

- (void)moveDragTo:(NSPoint)screenPoint {
  RenderViewHost* rvh = [contentsView_ tabContents]->render_view_host();
  if (rvh) {
    NSPoint localPoint = [contentsView_ convertPoint:screenPoint fromView:nil];
    FlipPointCoordinates(screenPoint, localPoint, contentsView_);
    rvh->DragSourceMovedTo(localPoint.x, localPoint.y,
                           screenPoint.x, screenPoint.y);
  }
}

@end  // @implementation WebContentsDragSource

