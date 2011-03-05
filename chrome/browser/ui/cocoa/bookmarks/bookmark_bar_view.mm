// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#include "chrome/browser/metrics/user_metrics.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "chrome/browser/themes/browser_theme_provider.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

@interface BookmarkBarView (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(ui::ThemeProvider*)themeProvider;
@end

@implementation BookmarkBarView

@synthesize dropIndicatorShown = dropIndicatorShown_;
@synthesize dropIndicatorPosition = dropIndicatorPosition_;
@synthesize noItemContainer = noItemContainer_;

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];
  [super dealloc];

  // To be clear, our controller_ is an IBOutlet and owns us, so we
  // don't deallocate it explicitly.  It is owned by the browser
  // window controller, so gets deleted with a browser window is
  // closed.
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kBrowserThemeDidChangeNotification
                      object:nil];

  DCHECK(controller_) << "Expected this to be hooked up via Interface Builder";
  NSArray* types = [NSArray arrayWithObjects:
                    NSStringPboardType,
                    NSHTMLPboardType,
                    NSURLPboardType,
                    kBookmarkButtonDragType,
                    kBookmarkDictionaryListPboardType,
                    nil];
  [self registerForDraggedTypes:types];
}

// We need the theme to color the bookmark buttons properly.  But our
// controller desn't have access to it until it's placed in the view
// hierarchy.  This is the spot where we close the loop.
- (void)viewWillMoveToWindow:(NSWindow*)window {
  ui::ThemeProvider* themeProvider = [window themeProvider];
  [self updateTheme:themeProvider];
  [controller_ updateTheme:themeProvider];
}

- (void)viewDidMoveToWindow {
  [controller_ viewDidMoveToWindow];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  ui::ThemeProvider* themeProvider =
      static_cast<ui::ThemeProvider*>([[aNotification object] pointerValue]);
  [self updateTheme:themeProvider];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(ui::ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;

  NSColor* color =
      themeProvider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT,
                                true);
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

-(NSButton*)importBookmarksButton {
  return importBookmarksButton_;
}

- (BookmarkBarController*)controller {
  return controller_;
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
    NSColor* uglyBlackBarColor = [[self window] themeProvider]->
        GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT, true);
    [[uglyBlackBarColor colorWithAlphaComponent:kBarOpacity] setFill];
    [[NSBezierPath bezierPathWithRect:uglyBlackBar] fill];
  }
}

// Shim function to assist in unit testing.
- (BOOL)dragClipboardContainsBookmarks {
  return bookmark_pasteboard_helper_mac::DragClipboardContainsBookmarks();
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  if ([[info draggingPasteboard] dataForType:kBookmarkButtonDragType] ||
      [self dragClipboardContainsBookmarks] ||
      [[info draggingPasteboard] containsURLData]) {
    // We only show the drop indicator if we're not in a position to
    // perform a hover-open since it doesn't make sense to do both.
    BOOL showIt = [controller_ shouldShowIndicatorShownForPoint:
                   [info draggingLocation]];
    if (!showIt) {
      if (dropIndicatorShown_) {
        dropIndicatorShown_ = NO;
        [self setNeedsDisplay:YES];
      }
    } else {
      CGFloat x =
      [controller_ indicatorPosForDragToPoint:[info draggingLocation]];
      // Need an update if the indicator wasn't previously shown or if it has
      // moved.
      if (!dropIndicatorShown_ || dropIndicatorPosition_ != x) {
        dropIndicatorShown_ = YES;
        dropIndicatorPosition_ = x;
        [self setNeedsDisplay:YES];
      }
    }

    [controller_ draggingEntered:info];  // allow hover-open to work.
    return [info draggingSource] ? NSDragOperationMove : NSDragOperationCopy;
  }
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
  return YES;
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
  [pboard getURLs:&urls andTitles:&titles convertingFilenames:YES];

  return [controller_ addURLs:urls
                   withTitles:titles
                           at:[info draggingLocation]];
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for bookmark buttons.
- (BOOL)performDragOperationForBookmarkButton:(id<NSDraggingInfo>)info {
  BOOL rtn = NO;
  NSData* data = [[info draggingPasteboard]
                  dataForType:kBookmarkButtonDragType];
  // [info draggingSource] is nil if not the same application.
  if (data && [info draggingSource]) {
    BookmarkButton* button = nil;
    [data getBytes:&button length:sizeof(button)];
    BOOL copy = !([info draggingSourceOperationMask] & NSDragOperationMove);
    rtn = [controller_ dragButton:button
                               to:[info draggingLocation]
                             copy:copy];
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBar_DragEnd"));
  }
  return rtn;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  if ([controller_ dragBookmarkData:info])
    return YES;
  NSPasteboard* pboard = [info draggingPasteboard];
  if ([pboard dataForType:kBookmarkButtonDragType]) {
    if ([self performDragOperationForBookmarkButton:info])
      return YES;
    // Fall through....
  }
  if ([pboard containsURLData]) {
    if ([self performDragOperationForURL:info])
      return YES;
  }
  return NO;
}

- (void)setController:(id)controller {
  controller_ = controller;
}

- (ViewID)viewID {
  return VIEW_ID_BOOKMARK_BAR;
}

@end  // @implementation BookmarkBarView
