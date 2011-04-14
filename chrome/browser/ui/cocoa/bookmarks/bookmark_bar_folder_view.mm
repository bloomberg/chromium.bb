// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_view.h"

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#include "chrome/browser/metrics/user_metrics.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

@implementation BookmarkBarFolderView

@synthesize dropIndicatorShown = dropIndicatorShown_;
@synthesize dropIndicatorPosition = dropIndicatorPosition_;

- (void)awakeFromNib {
  NSArray* types = [NSArray arrayWithObjects:
                    NSStringPboardType,
                    NSHTMLPboardType,
                    NSURLPboardType,
                    kBookmarkButtonDragType,
                    kBookmarkDictionaryListPboardType,
                    nil];
  [self registerForDraggedTypes:types];
}

- (void)dealloc {
  [self unregisterDraggedTypes];
  [super dealloc];
}

- (id<BookmarkButtonControllerProtocol>)controller {
  // When needed for testing, set the local data member |controller_| to
  // the test controller.
  return controller_ ? controller_ : [[self window] windowController];
}

- (void)setController:(id)controller {
  controller_ = controller;
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

// TODO(mrossetti,jrg): Identical to -[BookmarkBarView
// dragClipboardContainsBookmarks].  http://crbug.com/35966
// Shim function to assist in unit testing.
- (BOOL)dragClipboardContainsBookmarks {
  return bookmark_pasteboard_helper_mac::DragClipboardContainsBookmarks();
}

// Virtually identical to [BookmarkBarView draggingEntered:].
// TODO(jrg): find a way to share code.  Lack of multiple inheritance
// makes things more of a pain but there should be no excuse for laziness.
// http://crbug.com/35966
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  inDrag_ = YES;
  if (![[self controller] draggingAllowed:info])
    return NSDragOperationNone;
  if ([[info draggingPasteboard] dataForType:kBookmarkButtonDragType] ||
      [self dragClipboardContainsBookmarks] ||
      [[info draggingPasteboard] containsURLData]) {
    // Find the position of the drop indicator.
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
       indicatorPosForDragToPoint:[info draggingLocation]];

      // Need an update if the indicator wasn't previously shown or if it has
      // moved.
      if (!dropIndicatorShown_ || dropIndicatorPosition_ != y) {
        dropIndicatorShown_ = YES;
        dropIndicatorPosition_ = y;
        [self setNeedsDisplay:YES];
      }
    }

    [[self controller] draggingEntered:info];  // allow hover-open to work
    return [info draggingSource] ? NSDragOperationMove : NSDragOperationCopy;
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

// This code is practically identical to the same function in BookmarkBarView
// with the only difference being how the controller is retrieved.
// TODO(mrossetti,jrg): http://crbug.com/35966
// Implement NSDraggingDestination protocol method
// performDragOperation: for URLs.
- (BOOL)performDragOperationForURL:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  DCHECK([pboard containsURLData]);

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles convertingFilenames:YES];

  return [[self controller] addURLs:urls
                         withTitles:titles
                                 at:[info draggingLocation]];
}

// This code is practically identical to the same function in BookmarkBarView
// with the only difference being how the controller is retrieved.
// http://crbug.com/35966
// Implement NSDraggingDestination protocol method
// performDragOperation: for bookmark buttons.
- (BOOL)performDragOperationForBookmarkButton:(id<NSDraggingInfo>)info {
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
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBarFolder_DragEnd"));
  }
  return doDrag;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  if ([[self controller] dragBookmarkData:info])
    return YES;
  NSPasteboard* pboard = [info draggingPasteboard];
  if ([pboard dataForType:kBookmarkButtonDragType] &&
      [self performDragOperationForBookmarkButton:info])
    return YES;
  if ([pboard containsURLData] && [self performDragOperationForURL:info])
    return YES;
  return NO;
}

@end
