// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"

#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_context_menu_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/user_metrics.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

using base::UserMetricsAction;

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
  [super viewWillMoveToWindow:window];
}

- (void)viewDidMoveToWindow {
  [controller_ viewDidMoveToWindow];
}

// Called after a theme change took place, possibly for a different profile.
- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self updateTheme:[[self window] themeProvider]];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(ui::ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;

  NSColor* color =
      themeProvider->GetNSColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  [noItemTextfield_ setTextColor:color];
}

// Mouse down events on the bookmark bar should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (BookmarkBarTextField*)noItemTextfield {
  return noItemTextfield_;
}

- (NSButton*)importBookmarksButton {
  return importBookmarksButton_;
}

- (BookmarkBarController*)controller {
  return controller_;
}

// Internal method, needs to be called whenever a change has been made to
// dropIndicatorShown_ or dropIndicatorPosition_ so it can get the controller
// to reflect the change by moving buttons around.
- (void)dropIndicatorChanged {
  if (dropIndicatorShown_)
    [controller_ setDropInsertionPos:dropIndicatorPosition_];
  else
    [controller_ clearDropInsertionPos];
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  if (![controller_ draggingAllowed:info])
    return NSDragOperationNone;
  if ([[info draggingPasteboard] dataForType:kBookmarkButtonDragType] ||
      PasteboardContainsBookmarks(ui::CLIPBOARD_TYPE_DRAG) ||
      [[info draggingPasteboard] containsURLData]) {
    // We only show the drop indicator if we're not in a position to
    // perform a hover-open since it doesn't make sense to do both.
    BOOL showIt = [controller_ shouldShowIndicatorShownForPoint:
                   [info draggingLocation]];
    if (!showIt) {
      if (dropIndicatorShown_) {
        dropIndicatorShown_ = NO;
        [self dropIndicatorChanged];
      }
    } else {
      CGFloat x =
      [controller_ indicatorPosForDragToPoint:[info draggingLocation]];
      // Need an update if the indicator wasn't previously shown or if it has
      // moved.
      if (!dropIndicatorShown_ || dropIndicatorPosition_ != x) {
        dropIndicatorShown_ = YES;
        dropIndicatorPosition_ = x;
        [self dropIndicatorChanged];
      }
    }

    [controller_ draggingEntered:info];  // allow hover-open to work.
    return [[info draggingSource] isKindOfClass: [BookmarkButton class]] ?
        NSDragOperationMove : NSDragOperationCopy;
  }
  return NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  [controller_ draggingExited:info];

  // Regardless of the type of dragging which ended, we need to get rid of the
  // drop indicator if one was shown.
  if (dropIndicatorShown_) {
    dropIndicatorShown_ = NO;
    [self dropIndicatorChanged];
  }
}

- (void)draggingEnded:(id<NSDraggingInfo>)info {
  [controller_ draggingEnded:info];

  [[BookmarkButton draggedButton] setHidden:NO];
  if (dropIndicatorShown_) {
    dropIndicatorShown_ = NO;
    [self dropIndicatorChanged];
  }
  [controller_ draggingEnded:info];
}

- (BOOL)wantsPeriodicDraggingUpdates {
  return YES;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  // For now it's the same as draggingEntered:.
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

    // If we're dragging from one profile to another, disallow moving (only
    // allow copying). Each profile has its own bookmark model, so one way to
    // check whether we are dragging across profiles is to see if the
    // |BookmarkNode| corresponding to |button| exists in this profile. If it
    // does, we're dragging within a profile; otherwise, we're dragging across
    // profiles.
    const BookmarkModel* const model = [[self controller] bookmarkModel];
    const BookmarkNode* const source_node = [button bookmarkNode];
    const BookmarkNode* const target_node =
        bookmarks::GetBookmarkNodeByID(model, source_node->id());

    BOOL copy =
        !([info draggingSourceOperationMask] & NSDragOperationMove) ||
        (source_node != target_node);
    rtn = [controller_ dragButton:button
                               to:[info draggingLocation]
                             copy:copy];
    content::RecordAction(UserMetricsAction("BookmarkBar_DragEnd"));
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

- (NSMenu*)menu {
  return [[controller_ menuController] menuForBookmarkBar];
}

- (void)setController:(id)controller {
  controller_ = controller;
}

- (ViewID)viewID {
  return VIEW_ID_BOOKMARK_BAR;
}

@end  // @implementation BookmarkBarView

@implementation BookmarkBarTextField

- (NSMenu*)menu {
  return [barView_ menu];
}

@end  // @implementation BookmarkBarTextField

@implementation BookmarkBarItemContainer

- (NSMenu*)menu {
  return [barView_ menu];
}

@end  // @implementation BookmarkBarItemContainer
