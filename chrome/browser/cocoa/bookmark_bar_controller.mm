// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "skia/ext/skia_utils_mac.h"

@interface BookmarkBarController(Private)
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately;
- (void)showBookmarkBar:(BOOL)enable immediately:(BOOL)immediately;
@end

namespace {

// TODO(jrg): this is the right proportional height but overlaps the
// "blue outline" of the omnibox.  Fix.

// Our height, when opened.
const int kBookmarkBarHeight = 30;
// How much to adjust our parent view.
const int kBookmarkBarSuperviewHeightAdjustment = 25;
// How much to adjust the web frame.
const int kBookmarkBarWebframeHeightAdjustment = 25;

// Magic numbers from Cole
const CGFloat kDefaultBookmarkWidth = 150.0;
const CGFloat kBookmarkVerticalPadding = 2.0;
const CGFloat kBookmarkHorizontalPadding = 8.0;
};

@implementation BookmarkBarController

- (id)initWithProfile:(Profile*)profile
           parentView:(NSView*)parentView
       webContentView:(NSView*)webContentView
             delegate:(id<BookmarkURLOpener>)delegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:mac_util::MainAppBundle()])) {
    bookmarkModel_ = profile->GetBookmarkModel();
    preferences_ = profile->GetPrefs();
    parentView_ = parentView;
    webContentView_ = webContentView;
    delegate_ = delegate;
  }
  return self;
}

- (void)awakeFromNib {
  // We default to NOT open, which means height=0.
  DCHECK([[self view] isHidden]);  // Hidden so it's OK to change.
  NSRect frame = [[self view] frame];
  frame.size.height = 0;
  frame.size.width = [parentView_ frame].size.width;
  [[self view] setFrame:frame];

  // Make sure the nodes stay bottom-aligned.
  [[self view] setAutoresizingMask:(NSViewWidthSizable |
                                         NSViewMinYMargin)];
  // Be sure to enable the bar before trying to show it...
  barIsEnabled_ = YES;
  if (preferences_->GetBoolean(prefs::kShowBookmarkBar))
    [self showBookmarkBar:YES immediately:YES];

  // Don't pass ourself along (as 'self') until our init is completely
  // done.  Thus, this call is (almost) last.
  bridge_.reset(new BookmarkBarBridge(self, bookmarkModel_));
}

// Show or hide the bar based on the value of |show|. Handles
// animating the resize of the content view.  if |immediately| is YES,
// make changes immediately instead of using an animator.  If the bar
// is disabled, do absolutely nothing.  The routine which enables the
// bar will show it if relevant using other mechanisms (the pref) to
// determine desired state.
- (void)showBookmarkBar:(BOOL)show immediately:(BOOL)immediately {
  if (barIsEnabled_ && (barShouldBeShown_ != show)) {
    contentViewHasOffset_ = show;
    [[self view] setHidden:show ? NO : YES];
    barShouldBeShown_ = show;
    if (show) {
      [self loaded:bookmarkModel_];
    }
    [self applyContentAreaOffset:show immediately:immediately];
  }
}

// Apply a contents box offset to make (or remove) room for the
// bookmark bar.  If apply==YES, always make room (the contentView_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
//
// TODO(jrg): it is awkward we change the sizes of views for our
// parent and siblings; ideally they change their own sizes.
//
// TODO(jrg): unlike windows, we process events while an animator is
// running.  Thus, if you resize the window while the bookmark bar is
// animating, you'll mess things up.  Fix.
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately {
  if ([self view] == nil) {
    // We're too early, but awakeFromNib will call me again.
    return;
  }
  if (!contentViewHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  // None of these locals are members of the Hall of Justice.
  NSRect superframe = [parentView_ frame];
  NSRect frame = [[self view] frame];
  NSRect webframe = [webContentView_ frame];
  if (apply) {
    superframe.size.height += kBookmarkBarSuperviewHeightAdjustment;
    superframe.origin.y -= kBookmarkBarSuperviewHeightAdjustment;
    frame.size.height += kBookmarkBarHeight;
    webframe.size.height -= kBookmarkBarWebframeHeightAdjustment;
  } else {
    superframe.size.height -= kBookmarkBarSuperviewHeightAdjustment;
    superframe.origin.y += kBookmarkBarSuperviewHeightAdjustment;
    frame.size.height -= kBookmarkBarHeight;
    webframe.size.height += kBookmarkBarWebframeHeightAdjustment;
  }

  // TODO(jrg): Animators can be a little fussy.  Setting these three
  // off can sometimes causes races where the finish isn't as
  // expected.  Fix, or clean out the animators as an option.
  // Odd racing is FAR worse than a lack of an animator.
  if (1 /* immediately */) {
    [parentView_ setFrame:superframe];
    [webContentView_ setFrame:webframe];
    [[self view] setFrame:frame];
  } else {
    [[parentView_ animator] setFrame:superframe];
    [[webContentView_ animator] setFrame:webframe];
    [[[self view] animator] setFrame:frame];
  }

  [[self view] setNeedsDisplay:YES];
  [parentView_ setNeedsDisplay:YES];
  [webContentView_ setNeedsDisplay:YES];
}

- (BOOL)isBookmarkBarVisible {
  return barShouldBeShown_;
}

// We don't change a preference; we only change visibility.
// Preference changing (global state) is handled in
// BrowserWindowCocoa::ToggleBookmarkBar().
- (void)toggleBookmarkBar {
  [self showBookmarkBar:!barShouldBeShown_ immediately:YES];
}

- (void)setBookmarkBarEnabled:(BOOL)enabled {
  if (enabled) {
    // Enabling the bar; set enabled then show if needed.
    barIsEnabled_ = YES;
    if (preferences_->GetBoolean(prefs::kShowBookmarkBar))
      [self showBookmarkBar:YES immediately:YES];
  } else {
    // Disabling the bar; hide if visible.
    if ([self isBookmarkBarVisible]) {
      [self showBookmarkBar:NO immediately:YES];
    }
    barIsEnabled_ = NO;
  }
}

- (BookmarkNode*)nodeFromMenuItem:(id)menuItem {
  NSCell* cell = [[menuItem menu] delegate];
  BookmarkNode* node = static_cast<BookmarkNode*>(
      [[cell representedObject] pointerValue]);
  DCHECK(node);
  return node;
}

- (BookmarkNode*)nodeFromButton:(id)button {
  NSCell* cell = [button cell];
  BookmarkNode* node = static_cast<BookmarkNode*>(
      [[cell representedObject] pointerValue]);
  DCHECK(node);
  return node;
}

- (IBAction)openBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromButton:sender];
  [delegate_ openBookmarkURL:node->GetURL() disposition:CURRENT_TAB];
}

// As a convention we set the menu's delegate to be the button's cell
// so we can easily obtain bookmark info.  Convention applied in
// -[BookmarkButtonCell menu].

- (IBAction)openBookmarkInNewForegroundTab:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [delegate_ openBookmarkURL:node->GetURL() disposition:NEW_FOREGROUND_TAB];
}

- (IBAction)openBookmarkInNewWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [delegate_ openBookmarkURL:node->GetURL() disposition:NEW_WINDOW];
}

- (IBAction)openBookmarkInIncognitoWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [delegate_ openBookmarkURL:node->GetURL() disposition:OFF_THE_RECORD];
}

- (IBAction)deleteBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  bookmarkModel_->Remove(node->GetParent(),
                         node->GetParent()->IndexOfChild(node));
}

// Delete all items from the bookmark bar.  TODO(jrg): once the
// bookmark bar has other subviews (e.g. "off the side" button/menu,
// "Other Bookmarks"), etc, this routine will need revisiting.
- (void)clearBookmarkBar {
  [[self view] setSubviews:[NSArray array]];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell *)cellForBookmarkNode:(const BookmarkNode*)node frame:(NSRect)frame {
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSButtonCell *cell = [[[BookmarkButtonCell alloc] initTextCell:nil]
                         autorelease];
  DCHECK(cell);
  [cell setRepresentedObject:[NSValue valueWithPointer:node]];

  // The favicon may be NULL if we haven't loaded it yet.  Bookmarks
  // (and their icons) are loaded on the IO thread to speed launch.
  const SkBitmap& favicon = bookmarkModel_->GetFavIcon(node);
  if (!favicon.isNull()) {
    NSImage* image = gfx::SkBitmapToNSImage(favicon);
    if (image) {
      [cell setImage:image];
      [cell setImagePosition:NSImageLeft];
    }
  }
  [cell setTitle:title];
  [cell setMenu:buttonContextMenu_];
  return cell;
}

// TODO(jrg): accomodation for bookmarks less than minimum width in
// size (like Windows)?
- (NSRect)frameForBookmarkAtIndex:(int)index {
  NSRect bounds = [[self view] bounds];
  // TODO: be smarter about this; the animator delays the right height
  if (bounds.size.height == 0)
    bounds.size.height = kBookmarkBarHeight;

  NSRect frame = NSInsetRect(bounds,
                             kBookmarkHorizontalPadding,
                             kBookmarkVerticalPadding);
  frame.origin.x += (kDefaultBookmarkWidth * index);
  frame.size.width = kDefaultBookmarkWidth;
  return frame;
}

// Add all items from the given model to our bookmark bar.
// TODO(jrg): lots of things!
//  - bookmark folders (e.g. menu from the button)
//  - button and menu on the right for when bookmarks don't all fit on the
//    screen
//  - ...
//
// TODO(jrg): contextual menu (e.g. Open In New Tab) for each button
// in this function)
//
// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToBar:(const BookmarkNode*)node {
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSRect frame = [self frameForBookmarkAtIndex:i];
    NSButton* button = [[[NSButton alloc] initWithFrame:frame]
                         autorelease];
    DCHECK(button);
    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [button setCell:[self cellForBookmarkNode:child frame:frame]];

    if (child->is_folder()) {
      // For now just disable the button if it's a folder.
      // TODO(jrg): recurse.
      [button setEnabled:NO];
    } else {
      // Make the button do something
      [button setTarget:self];
      [button setAction:@selector(openBookmark:)];
      // Add a tooltip.
      NSString* title = base::SysWideToNSString(child->GetTitle());
      std::string url_string = child->GetURL().possibly_invalid_spec();
      NSString* tooltip = [NSString stringWithFormat:@"%@\n%s", title,
                                    url_string.c_str()];
      [button setToolTip:tooltip];
    }
    // Finally, add it to the bookmark bar.
    [[self view] addSubview:button];
  }
}

// TODO(jrg): for now this is brute force.
- (void)loaded:(BookmarkModel*)model {
  DCHECK(model == bookmarkModel_);
  // Do nothing if not active or too early
  if ((barShouldBeShown_ == NO) || !model->IsLoaded())
    return;
  // Else brute force nuke and build.
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [self clearBookmarkBar];
  [self addNodesToBar:node];
}

- (void)beingDeleted:(BookmarkModel*)model {
  [self clearBookmarkBar];
}

// TODO(jrg): for now this is brute force.
- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeRemoved:(BookmarkModel*)model
             parent:(const BookmarkNode*)oldParent index:(int)index {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node {
  [self loaded:model];
}

// TODO(jrg): linear searching is bad.
// Need a BookmarkNode-->NSCell mapping.
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  NSArray* views = [[self view] subviews];
  for (NSButton* button in views) {
    NSButtonCell* cell = [button cell];
    void* pointer = [[cell representedObject] pointerValue];
    const BookmarkNode* cellnode = static_cast<const BookmarkNode*>(pointer);
    if (cellnode == node) {
      NSImage* image = gfx::SkBitmapToNSImage(bookmarkModel_->GetFavIcon(node));
      if (image) {
        [cell setImage:image];
        [cell setImagePosition:NSImageLeft];
      }
      return;
    }
  }
}

// TODO(jrg): for now this is brute force.
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node {
  [self loaded:model];
}

- (void)setDelegate:(id<BookmarkURLOpener>)delegate {
  delegate_ = delegate;
}

@end
