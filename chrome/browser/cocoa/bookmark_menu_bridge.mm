// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import <AppKit/AppKit.h>
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "skia/ext/skia_utils_mac.h"

BookmarkMenuBridge::BookmarkMenuBridge(Profile* profile)
    : profile_(profile),
      controller_([[BookmarkMenuCocoaController alloc] initWithBridge:this]) {
  if (GetBookmarkModel())
    ObserveBookmarkModel();
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  BookmarkModel *model = GetBookmarkModel();
  if (model)
    model->RemoveObserver(this);
  [controller_ release];
}

NSMenu* BookmarkMenuBridge::BookmarkMenu() {
  NSMenu* bookmark_menu = [[[NSApp mainMenu] itemWithTag:IDC_BOOKMARK_MENU]
                            submenu];
  return bookmark_menu;
}

void BookmarkMenuBridge::Loaded(BookmarkModel* model) {
  NSMenu* bookmark_menu = BookmarkMenu();
  if (bookmark_menu == nil)
    return;

  ClearBookmarkMenu(bookmark_menu);

  // TODO(jrg): limit the number of bookmarks in the menubar?
  AddNodeToMenu(model->GetBookmarkBarNode(), bookmark_menu);
}

void BookmarkMenuBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  NSMenu* bookmark_menu = BookmarkMenu();
  if (bookmark_menu == nil)
    return;

  ClearBookmarkMenu(bookmark_menu);
}

void BookmarkMenuBridge::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           int old_index,
                                           const BookmarkNode* new_parent,
                                           int new_index) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int old_index,
                                             const BookmarkNode* node) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item);
}

void BookmarkMenuBridge::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item);
}

void BookmarkMenuBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

// Watch for changes.
void BookmarkMenuBridge::ObserveBookmarkModel() {
  BookmarkModel* model = GetBookmarkModel();
  model->AddObserver(this);
  if (model->IsLoaded())
    Loaded(model);
}

BookmarkModel* BookmarkMenuBridge::GetBookmarkModel() {
  if (!profile_)
    return NULL;
  return profile_->GetBookmarkModel();
}

Profile* BookmarkMenuBridge::GetProfile() {
  return profile_;
}

void BookmarkMenuBridge::ClearBookmarkMenu(NSMenu* menu) {
  bookmark_nodes_.clear();
  // Recursively delete all menus that look like a bookmark.  Assume
  // all items with submenus contain only bookmarks.  This typically
  // deletes everything except the first two items ("Add Bookmark..."
  // and separator)
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    // Convention: items in the bookmark list which are bookmarks have
    // an action of openBookmarkMenuItem:.  Also, assume all items
    // with submenus are submenus of bookmarks.
    if (([item action] == @selector(openBookmarkMenuItem:)) ||
        ([item hasSubmenu])) {
      // This will eventually [obj release] all its kids, if it has
      // any.
      [menu removeItem:item];
    } else {
      // Not a bookmark or item with submenu, so leave it alone.
    }
  }
}

void BookmarkMenuBridge::AddNodeToMenu(const BookmarkNode* node, NSMenu* menu) {
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    NSString* title = [BookmarkMenuCocoaController menuTitleForNode:child];
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                   action:nil
                                            keyEquivalent:@""] autorelease];
    [menu addItem:item];
    bookmark_nodes_[child] = item;
    if (child->is_folder()) {
      NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menu setSubmenu:submenu forItem:item];
      AddNodeToMenu(child, submenu);  // recursive call
    } else {
      ConfigureMenuItem(child, item);
    }
  }
}

void BookmarkMenuBridge::ConfigureMenuItem(const BookmarkNode* node,
                                           NSMenuItem* item) {
  [item setTarget:controller_];
  [item setAction:@selector(openBookmarkMenuItem:)];
  [item setTag:node->id()];
  // Add a tooltip
  std::string url_string = node->GetURL().possibly_invalid_spec();
  NSString* tooltip = [NSString stringWithFormat:@"%@\n%s",
                                base::SysWideToNSString(node->GetTitle()),
                                url_string.c_str()];
  [item setToolTip:tooltip];

  // Check to see if we have a favicon.
  NSImage* favicon = nil;
  BookmarkModel* model = GetBookmarkModel();
  if (model) {
    const SkBitmap& bitmap = model->GetFavIcon(node);
    if (!bitmap.isNull())
      favicon = gfx::SkBitmapToNSImage(bitmap);
  }
  // Either we do not have a loaded favicon or the conversion from SkBitmap
  // failed. Use the default site image instead.
  if (!favicon)
    favicon = nsimage_cache::ImageNamed(@"nav.pdf");
  [item setImage:favicon];
}

NSMenuItem* BookmarkMenuBridge::MenuItemForNode(const BookmarkNode* node) {
  if (!node)
    return nil;
  std::map<const BookmarkNode*, NSMenuItem*>::iterator it =
      bookmark_nodes_.find(node);
  if (it == bookmark_nodes_.end())
    return nil;
  return it->second;
}
