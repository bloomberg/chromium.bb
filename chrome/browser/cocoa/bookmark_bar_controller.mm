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
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/browser/profile.h"
#import "chrome/common/cocoa_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "skia/ext/skia_utils_mac.h"

@interface BookmarkBarController(Private)
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately;
- (void)showBookmarkBar:(BOOL)enable immediately:(BOOL)immediately;
- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu;
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu;
- (void)tagEmptyMenu:(NSMenu*)menu;
- (void)clearMenuTagMap;
@end

namespace {

// Our height, when opened.
const int kBookmarkBarHeight = 30;

// Magic numbers from Cole
const CGFloat kDefaultBookmarkWidth = 150.0;
const CGFloat kBookmarkVerticalPadding = 2.0;
const CGFloat kBookmarkHorizontalPadding = 1.0;
};

@implementation BookmarkBarController

- (id)initWithProfile:(Profile*)profile
         initialWidth:(float)initialWidth
       resizeDelegate:(id<ViewResizer>)resizeDelegate
          urlDelegate:(id<BookmarkURLOpener>)urlDelegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:mac_util::MainAppBundle()])) {
    profile_ = profile;
    initialWidth_ = initialWidth;
    bookmarkModel_ = profile->GetBookmarkModel();
    buttons_.reset([[NSMutableArray alloc] init]);
    resizeDelegate_ = resizeDelegate;
    urlDelegate_ = urlDelegate;
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)awakeFromNib {
  // We default to NOT open, which means height=0.
  DCHECK([[self view] isHidden]);  // Hidden so it's OK to change.

  // Set our initial height to zero, since that is what the superview
  // expects.  We will resize ourselves open later if needed.
  [[self view] setFrame:NSMakeRect(0, 0, initialWidth_, 0)];

  // We are enabled by default.
  barIsEnabled_ = YES;

  // Don't pass ourself along (as 'self') until our init is completely
  // done.  Thus, this call is (almost) last.
  bridge_.reset(new BookmarkBarBridge(self, bookmarkModel_));

  // When resized we may need to add new buttons, or remove them (if
  // no longer visible), or add/remove the "off the side" menu.
  [[self view] setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
    addObserver:self
       selector:@selector(frameDidChange)
           name:NSViewFrameDidChangeNotification
         object:[self view]];
}

- (void)showIfNeeded {
  if (profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
    [self showBookmarkBar:YES immediately:YES];
}

// Check if we should enable the off-the-side button.
// TODO(jrg): when we are smarter about creating buttons (e.g. don't
// bother creating buttons which aren't visible), we'll have to be
// smarter here too.
- (void)checkEnableOffTheSideButton {
  NSButton* button = [buttons_ lastObject];
  if ((!button) ||
      (NSMaxX([button frame]) <=
       NSMaxX([[button superview] frame]))) {
    [offTheSideButton_ setEnabled:NO];
  } else {
    [offTheSideButton_ setEnabled:YES];
  }
}

- (BOOL)offTheSideButtonIsEnabled {
  return [offTheSideButton_ isEnabled];
}

// Called when our controlled frame has changed size.
// TODO(jrg): be smarter (e.g. add/remove buttons as appropriate).
- (void)frameDidChange {
  [self checkEnableOffTheSideButton];
}

// Show or hide the bar based on the value of |show|. Handles
// animating the resize of the content view.  if |immediately| is YES,
// make changes immediately instead of using an animator.  If the bar
// is disabled, do absolutely nothing.  The routine which enables the
// bar will show it if relevant using other mechanisms (the pref) to
// determine desired state.
- (void)showBookmarkBar:(BOOL)show immediately:(BOOL)immediately {
  if (barIsEnabled_ && (barShouldBeShown_ != show)) {
    if ([self view]) {
      [[self view] setHidden:show ? NO : YES];
      [resizeDelegate_ resizeView:[self view]
                       newHeight:(show ? kBookmarkBarHeight : 0)];
    }
    barShouldBeShown_ = show;
    if (show) {
      [self loaded:bookmarkModel_];
    }
  }
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

// Return nil if menuItem has no delegate.
- (BookmarkNode*)nodeFromMenuItem:(id)menuItem {
  NSCell* cell = [[menuItem menu] delegate];
  if (!cell)
    return nil;
  BookmarkNode* node = static_cast<BookmarkNode*>(
      [[cell representedObject] pointerValue]);
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
  WindowOpenDisposition disposition = event_utils::DispositionFromEventFlags(
      [[NSApp currentEvent] modifierFlags]);
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:disposition];
}

// Given a NSMenuItem tag, return the appropriate bookmark node id.
- (int64)nodeIdFromMenuTag:(int32)tag {
  return menuTagMap_[tag];
}

// Create and return a new tag for the given node id.
- (int32)menuTagFromNodeId:(int64)menuid {
  int tag = seedId_++;
  menuTagMap_[tag] = menuid;
  return tag;
}

- (void)clearMenuTagMap {
  seedId_ = 0;
  menuTagMap_.clear();
}

// Recursively add the given bookmark node and all its children to
// menu, one menu item per node.
- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu {
  NSString* title = [BookmarkMenuCocoaController menuTitleForNode:child];
  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                 action:nil
                                          keyEquivalent:@""] autorelease];
  [menu addItem:item];
  if (child->is_folder()) {
    NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
    [menu setSubmenu:submenu forItem:item];
    if (child->GetChildCount()) {
      [self addFolderNode:child toMenu:submenu];  // potentially recursive
    } else {
      [self tagEmptyMenu:submenu];
    }
  } else {
    [item setTarget:self];
    [item setAction:@selector(openBookmarkMenuItem:)];
    [item setTag:[self menuTagFromNodeId:child->id()]];
    // Add a tooltip
    std::string url_string = child->GetURL().possibly_invalid_spec();
    NSString* tooltip = [NSString stringWithFormat:@"%@\n%s",
                                  base::SysWideToNSString(child->GetTitle()),
                                  url_string.c_str()];
    [item setToolTip:tooltip];

  }
}

// Empty menus are odd; if empty, add something to look at.
// Matches windows behavior.
// TODO(jrg): localize.
- (void)tagEmptyMenu:(NSMenu*)menu {
  [menu addItem:[[[NSMenuItem alloc] initWithTitle:@"(empty)"
                                            action:NULL
                                     keyEquivalent:@""] autorelease]];
}

// Add the children of the given bookmark node (and their children...)
// to menu, one menu item per node.
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu {
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    [self addNode:child toMenu:menu];
  }
}

// Return an autoreleased NSMenu that represents the given bookmark
// folder node.
- (NSMenu *)menuForFolderNode:(const BookmarkNode*)node {
  if (!node->is_folder())
    return nil;
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
  [self addFolderNode:node toMenu:menu];

  if (![menu numberOfItems]) {
    [self tagEmptyMenu:menu];
  }
  return menu;
}

// Called from a Folder bookmark button.
- (IBAction)openFolderMenuFromButton:(id)sender {
  NSMenu* menu = [self menuForFolderNode:[self nodeFromButton:sender]];
  if (menu) {
    [NSMenu popUpContextMenu:menu
                   withEvent:[NSApp currentEvent]
                     forView:sender];
  }
}

// TODO(jrg): cache the menu so we don't need to build it every time.
// TODO(jrg): if we get smarter such that we don't even bother
//   creating buttons which aren't visible, we'll need to be smarter
//   here.
- (IBAction)openOffTheSideMenuFromButton:(id)sender {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  for (NSButton* each_button in buttons_.get()) {
    if (NSMaxX([each_button frame]) >
        NSMaxX([[each_button superview] frame])) {
      [self addNode:[self nodeFromButton:each_button] toMenu:menu.get()];
    }
  }

  // TODO(jrg): once we disable the button when the menu should be
  // empty, remove this 'helper'.
  if (![menu numberOfItems]) {
    [self tagEmptyMenu:menu];
  }

  [NSMenu popUpContextMenu:menu
                 withEvent:[NSApp currentEvent]
                   forView:sender];
}

// As a convention we set the menu's delegate to be the button's cell
// so we can easily obtain bookmark info.  Convention applied in
// -[BookmarkButtonCell menu].

- (IBAction)openBookmarkInNewForegroundTab:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:NEW_FOREGROUND_TAB];
}

- (IBAction)openBookmarkInNewWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:NEW_WINDOW];
}

- (IBAction)openBookmarkInIncognitoWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:OFF_THE_RECORD];
}

- (IBAction)editBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];

  // TODO(jrg): on windows, folder "buttons" use the bar's context
  // menu (but with extra items enabled, like Rename).  For now we do
  // a cheat and redirect so we have the functionality available.
  if (node->is_folder()) {
    [self addOrRenameFolder:sender];
    return;
  }

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
      [urlDelegate_ openBookmarkURL:child->GetURL()
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

// May be called from the bar or from a folder button.
// If called from a button, that button becomes the parent.
- (IBAction)addPage:(id)sender {
  const BookmarkNode* parent = [self nodeFromMenuItem:sender];
  if (!parent)
    parent = bookmarkModel_->GetBookmarkBarNode();
  BookmarkEditor::Show([[[self view] window] contentView],
                       profile_,
                       parent,
                       nil,
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

// Might be from the context menu over the bar OR over a button.
- (IBAction)addOrRenameFolder:(id)sender {
  // node is NULL if we were invoked from the bar, and that's fine.
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  BookmarkNameFolderController* controller =
    [[BookmarkNameFolderController alloc]
      initWithParentWindow:[[self view] window]
                   profile:profile_
                      node:node];
  [controller runModal];

  // runModal will run the window as a sheet.  The
  // BookmarkNameFolderController will release itself when the sheet
  // ends.
}

- (NSView*)buttonView {
  return buttonView_;
}

// Delete all bookmarks from the bookmark bar, and reset knowledge of
// bookmarks.
- (void)clearBookmarkBar {
  [buttons_ makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [buttons_ removeAllObjects];
  [self clearMenuTagMap];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell*)cellForBookmarkNode:(const BookmarkNode*)node {
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSButtonCell *cell = [[[BookmarkButtonCell alloc] initTextCell:nil]
                         autorelease];
  DCHECK(cell);
  [cell setRepresentedObject:[NSValue valueWithPointer:node]];

  NSImage* image = NULL;
  if (node->is_folder()) {
    image = nsimage_cache::ImageNamed(@"bookmark_bar_folder.png");
  } else {
    const SkBitmap& favicon = bookmarkModel_->GetFavIcon(node);
    if (!favicon.isNull()) {
      image = gfx::SkBitmapToNSImage(favicon);
    }
  }
  if (image) {
    [cell setImage:image];
    [cell setImagePosition:NSImageLeft];
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

- (IBAction)openBookmarkMenuItem:(id)sender {
  int64 tag = [self nodeIdFromMenuTag:[sender tag]];
  const BookmarkNode* node = bookmarkModel_->GetNodeByID(tag);
  WindowOpenDisposition disposition = event_utils::DispositionFromEventFlags(
      [[NSApp currentEvent] modifierFlags]);
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:disposition];
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
      [button setTarget:self];
      [button setAction:@selector(openFolderMenuFromButton:)];
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
    [buttonView_ addSubview:button];
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
  [self checkEnableOffTheSideButton];
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

- (void)setUrlDelegate:(id<BookmarkURLOpener>)urlDelegate {
  urlDelegate_ = urlDelegate;
}

- (NSArray*)buttons {
  return buttons_.get();
}

@end
