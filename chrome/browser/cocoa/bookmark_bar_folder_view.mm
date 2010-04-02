// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_folder_view.h"

#import "chrome/browser/cocoa/bookmark_bar_controller.h"

@implementation BookmarkBarFolderView

- (id<BookmarkButtonControllerProtocol>)controller {
  return [[self window] windowController];
}

- (void)awakeFromNib {
  NSArray* types = [NSArray arrayWithObject:kBookmarkButtonDragType];
  [self registerForDraggedTypes:types];
}

- (void)dealloc {
  [self unregisterDraggedTypes];
  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  // TODO(jrg): copied from bookmark_bar_view but orientation changed.
  // Code dup sucks but I'm not sure I can take 16 lines and make it
  // generic for horiz vs vertical while keeping things simple.
  // TODO(jrg): when throwing it all away and using animations, try
  // hard to make a common routine for both.
  // http://crbug.com/35966, http://crbug.com/35968

  // Draw the bookmark-button-dragging drop indicator if necessary.
  if (dropIndicatorShown_) {
    const CGFloat kBarHeight = 1;
    const CGFloat kBarHorizPad = 4;
    const CGFloat kBarOpacity = 0.85;

    NSRect uglyBlackBar =
        NSMakeRect(kBarHorizPad, dropIndicatorPosition_,
                   NSWidth([self bounds]) - 2*kBarHorizPad,
                   kBarHeight);
    NSColor* uglyBlackBarColor = [NSColor blackColor];
    [[uglyBlackBarColor colorWithAlphaComponent:kBarOpacity] setFill];
    [[NSBezierPath bezierPathWithRect:uglyBlackBar] fill];
  }
}

// Virtually identical to [BookmarkBarView draggingEntered:].
// TODO(jrg): find a way to share code.  Lack of multiple inheritance
// makes things more of a pain but there should be no excuse for laziness.
// http://crbug.com/35966
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  inDrag_ = YES;

  NSData* data = [[info draggingPasteboard]
                     dataForType:kBookmarkButtonDragType];
  // [info draggingSource] is nil if not the same application.
  if (data && [info draggingSource]) {
    // Find the position of the drop indicator.
    BookmarkButton* button = nil;
    [data getBytes:&button length:sizeof(button)];

    BOOL showIt = [[self controller]
                      shouldShowIndicatorShownForPoint:[info draggingLocation]];
    if (!showIt) {
      if (dropIndicatorShown_) {
        dropIndicatorShown_ = NO;
        [self setNeedsDisplay:YES];
      }
    } else {
      CGFloat y =
          [[self controller]
              indicatorPosForDragOfButton:button
                                  toPoint:[info draggingLocation]];

      // Need an update if the indicator wasn't previously shown or if it has
      // moved.
      if (!dropIndicatorShown_ || dropIndicatorPosition_ != y) {
        dropIndicatorShown_ = YES;
        dropIndicatorPosition_ = y;
        [self setNeedsDisplay:YES];
      }
    }

    [[self controller] draggingEntered:info];  // allow hover-open to work
    return NSDragOperationMove;
  }

  return NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  [[self controller] draggingExited:info];

  // Regardless of the type of dragging which ended, we need to get rid of the
  // drop indicator if one was shown.
  if (dropIndicatorShown_) {
    dropIndicatorShown_ = NO;
    [self setNeedsDisplay:YES];
  }
}

- (void)draggingEnded:(id<NSDraggingInfo>)info {
  // Awkwardness since views open and close out from under us.
  if (inDrag_) {
    inDrag_ = NO;

    // This line makes sure menus get closed when a drag isn't
    // completed.
    [[self controller] closeAllBookmarkFolders];
  }

  [self draggingExited:info];
}

- (BOOL)wantsPeriodicDraggingUpdates {
  // TODO(jrg): This should probably return |YES| and the controller should
  // slide the existing bookmark buttons interactively to the side to make
  // room for the about-to-be-dropped bookmark.
  // http://crbug.com/35968
  return NO;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  // For now it's the same as draggingEntered:.
  // TODO(jrg): once we return YES for wantsPeriodicDraggingUpdates,
  // this should ping the [self controller] to perform animations.
  // http://crbug.com/35968
  return [self draggingEntered:info];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info {
  return YES;
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for bookmarks.
- (BOOL)performDragOperationForBookmark:(id<NSDraggingInfo>)info {
  BOOL doDrag = NO;
  NSData* data = [[info draggingPasteboard]
                   dataForType:kBookmarkButtonDragType];
  // [info draggingSource] is nil if not the same application.
  if (data && [info draggingSource]) {
    BookmarkButton* button = nil;
    [data getBytes:&button length:sizeof(button)];
    BOOL copy = !([info draggingSourceOperationMask] & NSDragOperationMove);
    doDrag = [[self controller] dragButton:button
                                        to:[info draggingLocation]
                                      copy:copy];
  }
  return doDrag;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  if ([pboard dataForType:kBookmarkButtonDragType]) {
    if ([self performDragOperationForBookmark:info])
      return YES;
    // Fall through....
  }
  return NO;
}

@end  // BookmarkBarFolderView


@implementation BookmarkBarFolderView(TestingAPI)

- (void)setDropIndicatorShown:(BOOL)shown {
  dropIndicatorShown_ = shown;
}

@end
