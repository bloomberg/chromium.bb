// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/menu_button.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

// Specialization of NSButton that responds to middle-clicks. By default,
// NSButton ignores them.
@interface BookmarkButton : NSButton
@end

@implementation BookmarkButton
- (void)otherMouseUp:(NSEvent*) event {
  [self performClick:self];
}
@end

@interface BookmarkBarController(Private)
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately;
- (void)showBookmarkBar:(BOOL)enable immediately:(BOOL)immediately;
- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu;
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu;
- (void)tagEmptyMenu:(NSMenu*)menu;
- (void)clearMenuTagMap;
- (int)preferredHeight;
@end

@implementation BookmarkBarController

- (id)initWithBrowser:(Browser*)browser
         initialWidth:(float)initialWidth
     compressDelegate:(id<ToolbarCompressable>)compressDelegate
       resizeDelegate:(id<ViewResizer>)resizeDelegate
          urlDelegate:(id<BookmarkURLOpener>)urlDelegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:mac_util::MainAppBundle()])) {
    browser_ = browser;
    initialWidth_ = initialWidth;
    bookmarkModel_ = browser_->profile()->GetBookmarkModel();
    buttons_.reset([[NSMutableArray alloc] init]);
    compressDelegate_ = compressDelegate;
    resizeDelegate_ = resizeDelegate;
    urlDelegate_ = urlDelegate;
    tabObserver_.reset(
        new TabStripModelObserverBridge(browser_->tabstrip_model(), self));

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folderImage_.reset([rb.GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER) retain]);
  }
  return self;
}

- (void)dealloc {
  bridge_.reset(NULL);
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

  DCHECK([offTheSideButton_ attachedMenu]);
}

// Method is the same as [self view], but is provided to be explicit.
- (BackgroundGradientView*)backgroundGradientView {
  return (BackgroundGradientView*)[self view];
}

- (void)showIfNeeded {
  if (browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
    [self showBookmarkBar:YES immediately:YES];
}

// Check if we should enable the off-the-side button.
// TODO(jrg): when we are smarter about creating buttons (e.g. don't
// bother creating buttons which aren't visible), we'll have to be
// smarter here too.
- (void)checkHideOffTheSideButton {
  NSButton* button = [buttons_ lastObject];
  if ((!button) ||
      (NSMaxX([button frame]) <=
       NSMaxX([[button superview] frame]))) {
    [offTheSideButton_ setHidden:YES];
  } else {
    [offTheSideButton_ setHidden:NO];
  }
}

- (BOOL)offTheSideButtonIsHidden {
  return [offTheSideButton_ isHidden];
}

// Called when our controlled frame has changed size.
// TODO(jrg): be smarter (e.g. add/remove buttons as appropriate).
- (void)frameDidChange {
  [self checkHideOffTheSideButton];

  // Need to keep the "no items" label centered.
  NSView* noItemsView = [buttonView_ noItemTextfield];
  NSRect frame = [noItemsView frame];
  NSRect parent = [buttonView_ bounds];
  NSPoint newOrigin;
  newOrigin.x = parent.origin.x + bookmarks::kNoBookmarksHorizontalOffset;
  newOrigin.y = parent.origin.y + parent.size.height -
      ([self isAlwaysVisible] ? bookmarks::kNoBookmarksVerticalOffset :
                                bookmarks::kNoBookmarksNTPVerticalOffset);
  [noItemsView setFrameOrigin:newOrigin];
}

// Show or hide the bar based on the value of |show|. Handles animating the
// resize of the content view.  if |immediately| is YES, make changes
// immediately instead of using an animator. The routine which enables the bar
// will show it if relevant using other mechanisms (the pref) to determine
// desired state.
- (void)showBookmarkBar:(BOOL)show immediately:(BOOL)immediately {
  BOOL compressed = [self isAlwaysVisible];
  [compressDelegate_ setShouldBeCompressed:compressed];

  CGFloat height = show ? [self preferredHeight] : 0;
  [resizeDelegate_ resizeView:[self view] newHeight:height];
  [[self view] setHidden:show ? NO : YES];

  DCHECK([[self view] isKindOfClass:[BookmarkBarToolbarView class]]);
  [(BookmarkBarToolbarView*)[self view] layoutViews];
}

// We don't change a preference; we only change visibility.
// Preference changing (global state) is handled in
// BrowserWindowCocoa::ToggleBookmarkBar(). We simply update the visibility of
// the bar based on the current value of the pref.
- (void)updateVisibility {
  [self showBookmarkBar:[self isVisible] immediately:YES];
}

- (void)setBookmarkBarEnabled:(BOOL)enabled {
  barIsEnabled_ = enabled ? YES : NO;
  [self updateVisibility];
}

- (BOOL)isVisible {
  return ([self isAlwaysVisible] && barIsEnabled_) || [self isNewTabPage];
}

- (BOOL)isNewTabPage {
  return browser_ && browser_->GetSelectedTabContents() &&
      browser_->GetSelectedTabContents()->ShouldShowBookmarkBar();
}

- (BOOL)isAlwaysVisible {
  return browser_ &&
      browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

- (int)currentTabContentsHeight {
  return browser_->GetSelectedTabContents()->view()->GetContainerSize().
      height();
}

- (ThemeProvider*)themeProvider {
  return browser_->profile()->GetThemeProvider();
}

- (BOOL)drawAsFloatingBar {
  return ![self isAlwaysVisible] && [self isNewTabPage];
}

// Return nil if menuItem has no delegate.
- (BookmarkNode*)nodeFromMenuItem:(id)menuItem {
  NSCell* cell = reinterpret_cast<NSCell*>([[menuItem menu] delegate]);
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
  [urlDelegate_ openBookmarkURL:node->GetURL()
                    disposition:event_utils::WindowOpenDispositionFromNSEvent(
                        [NSApp currentEvent])];
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

- (int)preferredHeight {
  return [self isAlwaysVisible] ? bookmarks::kBookmarkBarHeight :
      bookmarks::kNTPBookmarkBarHeight;
}

- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  // We need selectTabWithContents: for when we change from a tab that is the
  // new tab page to a tab that isn't.
  [self updateVisibility];
}

- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index
                   loadingOnly:(BOOL)loading {
  // We need tabChangedWithContents: for when the user clicks a bookmark from
  // the bookmark bar on the new tab page.
  [self updateVisibility];
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
    const SkBitmap& favicon = bookmarkModel_->GetFavIcon(child);
    if (!favicon.isNull()) {
      NSImage* image = gfx::SkBitmapToNSImage(favicon);
      if (image) {
        [item setImage:image];
      }
    }
  }
}

// Empty menus are odd; if empty, add something to look at.
// Matches windows behavior.
- (void)tagEmptyMenu:(NSMenu*)menu {
  NSString* empty_menu_title = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
  [menu addItem:[[[NSMenuItem alloc] initWithTitle:empty_menu_title
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

// Rebuild the off-the-side menu, taking into account which buttons are
// displayed.
// TODO(jrg,viettrungluu): only (re)build the menu when necessary.
// TODO(jrg): if we get smarter such that we don't even bother
//   creating buttons which aren't visible, we'll need to be smarter
//   here.
- (void)buildOffTheSideMenu {
  NSMenu* menu = [self offTheSideMenu];
  DCHECK(menu);

  // Remove old menu items (backwards order is as good as any); leave the
  // blank one at position 0 (see menu_button.h).
  for (NSInteger i = [menu numberOfItems] - 1; i >= 1 ; i--)
    [menu removeItemAtIndex:i];

  // Add items corresponding to buttons which aren't displayed or are only
  // partly displayed.
  for (NSButton* each_button in buttons_.get()) {
    if (NSMaxX([each_button frame]) >
        NSMaxX([[each_button superview] frame])) {
      [self addNode:[self nodeFromButton:each_button] toMenu:menu];
    }
  }
}

// Get the off-the-side menu.
- (NSMenu*)offTheSideMenu {
  return [offTheSideButton_ attachedMenu];
}

// Called by any menus which have set us as their delegate (right now just the
// off-the-side menu?).
- (void)menuNeedsUpdate:(NSMenu*)menu {
  if (menu == [self offTheSideMenu])
    [self buildOffTheSideMenu];
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
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
                       node->GetParent(),
                       node,
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

- (IBAction)copyBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  const std::string spec = node->GetURL().spec();
  NSString* url = base::SysUTF8ToNSString(spec);
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard declareURLPasteboardWithAdditionalTypes:[NSArray array]
                                                owner:nil];
  [pasteboard setDataForURL:url title:title];
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
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
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
                   profile:browser_->profile()
                      node:node];
  [controller runAsModalSheet];

  // runAsModalSheet will run the window as a sheet.  The
  // BookmarkNameFolderController will release itself when the sheet
  // ends.
}

- (BookmarkBarView*)buttonView {
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
    image = folderImage_;
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
// The "+2" is needed because, sometimes, Cocoa is off by a tad.
// Example: for a bookmark named "Moma" or "SFGate", it is one pixel
// too small.  For "FBL" it is 2 pixels too small.
// For a bookmark named "SFGateFooWoo", it is just fine.
- (CGFloat)widthForBookmarkButtonCell:(NSCell*)cell {
  CGFloat desired = [cell cellSize].width + 2;
  return std::min(desired, bookmarks::kDefaultBookmarkWidth);
}

// Returns a frame appropriate for the given bookmark cell, suitable
// for creating an NSButton that will contain it.  |xOffset| is the X
// offset for the frame; it is increased to be an appropriate X offset
// for the next button.
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell
                                 xOffset:(int*)xOffset {
  NSRect bounds = [buttonView_ bounds];
  // TODO(erg,jrg): There used to be an if statement here, comparing the height
  // to 0. This essentially broke sizing because we're dealing with floats and
  // the height wasn't precisely zero. The previous author wrote that they were
  // doing this because of an animator, but we are not doing animations for beta
  // so we do not care.
  bounds.size.height = bookmarks::kBookmarkBarHeight;

  NSRect frame = NSInsetRect(bounds,
                             bookmarks::kBookmarkHorizontalPadding,
                             bookmarks::kBookmarkVerticalPadding);
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
  // We may have just crossed a threshold to enable the off-the-side
  // button.
  [self checkHideOffTheSideButton];
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  int64 tag = [self nodeIdFromMenuTag:[sender tag]];
  const BookmarkNode* node = bookmarkModel_->GetNodeByID(tag);
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  [urlDelegate_ openBookmarkURL:node->GetURL() disposition:disposition];
}

// Add all items from the given model to our bookmark bar.
//
// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToBar:(const BookmarkNode*)node {
  BOOL hidden = (node->GetChildCount() == 0) ? NO : YES;
  NSView* item = [buttonView_ noItemTextfield];
  [item setHidden:hidden];

  int x_offset = 0;
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSCell* cell = [self cellForBookmarkNode:child];
    NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&x_offset];
    NSButton* button = [[[BookmarkButton alloc] initWithFrame:frame]
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
  if (!model->IsLoaded())
    return;
  // Else brute force nuke and build.
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [self clearBookmarkBar];
  [self addNodesToBar:node];
  [self checkHideOffTheSideButton];
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
