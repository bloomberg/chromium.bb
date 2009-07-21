// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
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
const CGFloat kBookmarkHorizontalPadding = 1.0;
};

@implementation BookmarkBarController

- (id)initWithProfile:(Profile*)profile
           parentView:(NSView*)parentView
       webContentView:(NSView*)webContentView
         infoBarsView:(NSView*)infoBarsView
             delegate:(id<BookmarkURLOpener>)delegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:mac_util::MainAppBundle()])) {
    profile_ = profile;
    bookmarkModel_ = profile->GetBookmarkModel();
    parentView_ = parentView;
    webContentView_ = webContentView;
    infoBarsView_ = infoBarsView;
    buttons_.reset([[NSMutableArray alloc] init]);
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
  if (profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
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
  NSRect infoframe = [infoBarsView_ frame];
  if (apply) {
    superframe.size.height += kBookmarkBarSuperviewHeightAdjustment;
    // TODO(jrg): y=0 if we add the bookmark bar before the parent
    // view (toolbar) is placed in the view hierarchy.  A different
    // CL, where the bookmark bar is extracted from the toolbar nib,
    // may fix this awkwardness.
    if (superframe.origin.y > 0) {
      superframe.origin.y -= kBookmarkBarSuperviewHeightAdjustment;
      webframe.size.height -= kBookmarkBarWebframeHeightAdjustment;
    }
    frame.size.height += kBookmarkBarHeight;
    infoframe.origin.y -= kBookmarkBarWebframeHeightAdjustment;
  } else {
    superframe.size.height -= kBookmarkBarSuperviewHeightAdjustment;
    superframe.origin.y += kBookmarkBarSuperviewHeightAdjustment;
    frame.size.height -= kBookmarkBarHeight;
    webframe.size.height += kBookmarkBarWebframeHeightAdjustment;
    infoframe.origin.y += kBookmarkBarWebframeHeightAdjustment;
  }

  // TODO(jrg): Animators can be a little fussy.  Setting these three
  // off can sometimes causes races where the finish isn't as
  // expected.  Fix, or clean out the animators as an option.
  // Odd racing is FAR worse than a lack of an animator.
  if (1 /* immediately */) {
    [parentView_ setFrame:superframe];
    [webContentView_ setFrame:webframe];
    [infoBarsView_ setFrame:infoframe];
    [[self view] setFrame:frame];
  } else {
    [[parentView_ animator] setFrame:superframe];
    [[webContentView_ animator] setFrame:webframe];
    [[infoBarsView_ animator] setFrame:infoframe];
    [[[self view] animator] setFrame:frame];
  }

  [[self view] setNeedsDisplay:YES];
  [parentView_ setNeedsDisplay:YES];
  [webContentView_ setNeedsDisplay:YES];
  [infoBarsView_ setNeedsDisplay:YES];
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
    if (profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
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

- (IBAction)editBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  // There is no real need to jump to a platform-common routine at
  // this point (which just jumps back to objc) other than consistency
  // across platforms.
  //
  // TODO(jrg): identify when we NO_TREE.  I can see it in the code
  // for the other platforms but can't find a way to trigger it in the
  // UI.
  BookmarkEditor::Show([[[self view] window] contentView],
                       profile_,
                       node->GetParent(),
                       node,
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

- (IBAction)deleteBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  bookmarkModel_->Remove(node->GetParent(),
                         node->GetParent()->IndexOfChild(node));
}

- (void)openBookmarkNodesRecursive:(BookmarkNode*)node {
  for (int i = 0; i < node->GetChildCount(); i++) {
    BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      [delegate_ openBookmarkURL:child->GetURL()
                     disposition:NEW_BACKGROUND_TAB];
    else
      [self openBookmarkNodesRecursive:child];
  }
}

- (IBAction)openAllBookmarks:(id)sender {
  // TODO(jrg):
  // Is there an easier way to get a non-const root node for the bookmark bar?
  // I can't iterate over them unless it's non-const.

  BookmarkNode* node = (BookmarkNode*)bookmarkModel_->GetBookmarkBarNode();
  [self openBookmarkNodesRecursive:node];
}

- (IBAction)addPage:(id)sender {
  BookmarkEditor::Show([[[self view] window] contentView],
                       profile_,
                       bookmarkModel_->GetBookmarkBarNode(),
                       nil,
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

// Delete all bookmarks from the bookmark bar.
- (void)clearBookmarkBar {
  [buttons_ makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [buttons_ removeAllObjects];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell*)cellForBookmarkNode:(const BookmarkNode*)node {
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

// Return an appropriate width for the given bookmark button cell.
// The "+1" is needed because, sometimes, Cocoa is off by one.
// Example: for a bookmark named "Moma" or "SFGate", it is one pixel
// too small.  For a bookmark named "SFGateFooWoo", it is just fine.
- (CGFloat)widthForBookmarkButtonCell:(NSCell*)cell {
  CGFloat desired = [cell cellSize].width + 1;
  return std::min(desired, kDefaultBookmarkWidth);
}

// Returns a frame appropriate for the given bookmark cell, suitable
// for creating an NSButton that will contain it.  |xOffset| is the X
// offset for the frame; it is increased to be an appropriate X offset
// for the next button.
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell
                                 xOffset:(int*)xOffset {
  NSRect bounds = [[self view] bounds];
  // TODO: be smarter about this; the animator delays the right height
  if (bounds.size.height == 0)
    bounds.size.height = kBookmarkBarHeight;
  NSRect frame = NSInsetRect(bounds,
                             kBookmarkHorizontalPadding,
                             kBookmarkVerticalPadding);
  frame.size.width = [self widthForBookmarkButtonCell:cell];

  // Add an X offset based on what we've already done
  frame.origin.x += *xOffset;

  // And up the X offset for next time.
  *xOffset = NSMaxX(frame);

  return frame;
}

// A bookmark button's contents changed.  Check for growth
// (e.g. increase the width up to the maximum).  If we grew, move
// other bookmark buttons over.
- (void)checkForBookmarkButtonGrowth:(NSButton*)button {
  NSRect frame = [button frame];
  CGFloat desiredSize = [self widthForBookmarkButtonCell:[button cell]];
  CGFloat delta = desiredSize - frame.size.width;
  if (delta) {
    frame.size.width = desiredSize;
    [button setFrame:frame];
    for (NSButton* each_button in buttons_.get()) {
      NSRect each_frame = [each_button frame];
      if (each_frame.origin.x > frame.origin.x) {
        each_frame.origin.x += delta;
        [each_button setFrame:each_frame];
      }
    }
  }
}

// Add all items from the given model to our bookmark bar.
// TODO(jrg): lots of things!
//  - bookmark folders (e.g. menu from the button)
//  - button and menu on the right for when bookmarks don't all fit on the
//    screen
//  - ...
//
// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToBar:(const BookmarkNode*)node {
  int x_offset = 0;
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSCell* cell = [self cellForBookmarkNode:child];
    NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&x_offset];
    NSButton* button = [[[NSButton alloc] initWithFrame:frame]
                         autorelease];
    DCHECK(button);
    [buttons_ addObject:button];

    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [button setCell:cell];

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
//
// TODO(jrg): if the bookmark bar is open on launch, we see the
// buttons all placed, then "scooted over" as the favicons load.  If
// this looks bad I may need to change widthForBookmarkButtonCell to
// add space for an image even if not there on the assumption that
// favicons will eventually load.
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  for (NSButton* button in buttons_.get()) {
    NSButtonCell* cell = [button cell];
    void* pointer = [[cell representedObject] pointerValue];
    const BookmarkNode* cellnode = static_cast<const BookmarkNode*>(pointer);
    if (cellnode == node) {
      NSImage* image = gfx::SkBitmapToNSImage(bookmarkModel_->GetFavIcon(node));
      if (image) {
        [cell setImage:image];
        [cell setImagePosition:NSImageLeft];
        // Adding an image means we might need more room for the
        // bookmark.  Test for it by growing the button (if needed)
        // and shifting everything else over.
        [self checkForBookmarkButtonGrowth:button];
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

- (NSArray*)buttons {
  return buttons_.get();
}

@end
