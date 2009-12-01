// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_view.h"

#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

@interface BookmarkBarView (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(GTMTheme*)theme;
@end

@implementation BookmarkBarView

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];
  [super dealloc];
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kGTMThemeDidChangeNotification
                      object:nil];

  DCHECK(controller_ && "Expected this to be hooked up via Interface Builder");
  NSArray* types = [NSArray arrayWithObjects:
                              NSStringPboardType,
                              NSHTMLPboardType,
                              NSURLPboardType,
                              kBookmarkButtonDragType,
                              nil];
  [self registerForDraggedTypes:types];
}

// We need the theme to color the bookmark buttons properly.  But our
// controller desn't have access to it until it's placed in the view
// hierarchy.  This is the spot where we close the loop.
- (void)viewWillMoveToWindow:(NSWindow*)window {
  [self updateTheme:[window gtm_theme]];
  [controller_ updateTheme:[window gtm_theme]];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  GTMTheme* theme = [aNotification object];
  [self updateTheme:theme];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(GTMTheme*)theme {
  NSColor* color = [theme textColorForStyle:GTMThemeStyleBookmarksBarButton
                                      state:GTMThemeStateActiveWindow];
  [noItemTextfield_ setTextColor:color];
}

// Mouse down events on the bookmark bar should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

-(NSTextField*)noItemTextfield {
  return noItemTextfield_;
}

-(void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  // Draw the bookmark-button-dragging drop indicator if necessary.
  if (dropIndicatorShown_) {
    const CGFloat kBarWidth = 1;
    const CGFloat kBarHalfWidth = kBarWidth / 2.0;
    const CGFloat kBarVertPad = 4;
    const CGFloat kBarOpacity = 0.85;

    // Prevent the indicator from being clipped on the left.
    CGFloat xLeft = MAX(dropIndicatorPosition_ - kBarHalfWidth, 0);

    NSRect uglyBlackBar =
        NSMakeRect(xLeft, kBarVertPad,
                   kBarWidth, NSHeight([self bounds]) - 2 * kBarVertPad);
    NSColor* uglyBlackBarColor =
        [[self gtm_theme] textColorForStyle:GTMThemeStyleBookmarksBarButton
                                      state:GTMThemeStateActiveWindow];
    [[uglyBlackBarColor colorWithAlphaComponent:kBarOpacity] setFill];
    [[NSBezierPath bezierPathWithRect:uglyBlackBar] fill];
  }
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  if ([[info draggingPasteboard] dataForType:kBookmarkButtonDragType]) {
    NSData* data = [[info draggingPasteboard]
                     dataForType:kBookmarkButtonDragType];
    // [info draggingSource] is nil if not the same application.
    if (data && [info draggingSource]) {
      // Find the position of the drop indicator.
      BookmarkButton* button = nil;
      [data getBytes:&button length:sizeof(button)];
      CGFloat x =
          [controller_ indicatorPosForDragOfButton:button
                                           toPoint:[info draggingLocation]];

      // Need an update if the indicator wasn't previously shown or if it has
      // moved.
      if (!dropIndicatorShown_ || dropIndicatorPosition_ != x) {
        dropIndicatorShown_ = YES;
        dropIndicatorPosition_ = x;
        [self setNeedsDisplay:YES];
      }

      return NSDragOperationMove;
    }
    // Fall through otherwise.
  }
  if ([[info draggingPasteboard] containsURLData])
    return NSDragOperationCopy;
  return NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  // Regardless of the type of dragging which ended, we need to get rid of the
  // drop indicator if one was shown.
  if (dropIndicatorShown_) {
    dropIndicatorShown_ = NO;
    [self setNeedsDisplay:YES];
  }
}

- (void)draggingEnded:(id<NSDraggingInfo>)info {
  // For now, we just call |-draggingExited:|.
  [self draggingExited:info];
}

- (BOOL)wantsPeriodicDraggingUpdates {
  // TODO(port): This should probably return |YES| and the controller should
  // slide the existing bookmark buttons interactively to the side to make
  // room for the about-to-be-dropped bookmark.
  return NO;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  // For now it's the same as draggingEntered:.
  // TODO(jrg): once we return YES for wantsPeriodicDraggingUpdates,
  // this should ping the controller_ to perform animations.
  return [self draggingEntered:info];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info {
  return YES;
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for URLs.
- (BOOL)performDragOperationForURL:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  DCHECK([pboard containsURLData]);

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles];

  return [controller_ addURLs:urls
                   withTitles:titles
                           at:[info draggingLocation]];
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for bookmarks.
- (BOOL)performDragOperationForBookmark:(id<NSDraggingInfo>)info {
  BOOL rtn = NO;
  NSData* data = [[info draggingPasteboard]
                   dataForType:kBookmarkButtonDragType];
  // [info draggingSource] is nil if not the same application.
  if (data && [info draggingSource]) {
    BookmarkButton* button = nil;
    [data getBytes:&button length:sizeof(button)];
    rtn = [controller_ dragButton:button to:[info draggingLocation]];
  }
  return rtn;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  if ([pboard dataForType:kBookmarkButtonDragType]) {
    if ([self performDragOperationForBookmark:info])
      return YES;
    // Fall through....
  }
  if ([pboard containsURLData]) {
    if ([self performDragOperationForURL:info])
      return YES;
  }
  return NO;
}

@end  // @implementation BookmarkBarView


@implementation BookmarkBarView(TestingAPI)

- (void)setController:(id)controller {
  controller_ = controller;
}

@end  // @implementation BookmarkBarView(TestingAPI)
