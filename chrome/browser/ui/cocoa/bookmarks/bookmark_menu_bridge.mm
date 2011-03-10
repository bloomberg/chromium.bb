// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "app/mac/nsimage_cache.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

BookmarkMenuBridge::BookmarkMenuBridge(Profile* profile)
    : menuIsValid_(false),
      profile_(profile),
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
  return [controller_ menu];
}

void BookmarkMenuBridge::Loaded(BookmarkModel* model) {
  InvalidateMenu();
}

void BookmarkMenuBridge::UpdateMenu(NSMenu* bookmark_menu) {
  DCHECK(bookmark_menu);
  if (menuIsValid_)
    return;
  BookmarkModel* model = GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;

  if (!folder_image_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folder_image_.reset(
        [rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER) retain]);
  }

  ClearBookmarkMenu(bookmark_menu);

  // Add bookmark bar items, if any.
  const BookmarkNode* barNode = model->GetBookmarkBarNode();
  CHECK(barNode);
  if (barNode->child_count()) {
    [bookmark_menu addItem:[NSMenuItem separatorItem]];
    AddNodeToMenu(barNode, bookmark_menu);
  }

  // Create a submenu for "other bookmarks", and fill it in.
  NSString* other_items_title =
      l10n_util::GetNSString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME);
  [bookmark_menu addItem:[NSMenuItem separatorItem]];
  AddNodeAsSubmenu(bookmark_menu,
                   model->other_node(),
                   other_items_title);

  menuIsValid_ = true;
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
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int old_index,
                                             const BookmarkNode* node) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item, true);
}

void BookmarkMenuBridge::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item, false);
}

void BookmarkMenuBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  InvalidateMenu();
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
  // Recursively delete all menus that look like a bookmark. Also delete all
  // separator items since we explicitly add them back in. This deletes
  // everything except the first item ("Add Bookmark...").
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    // Convention: items in the bookmark list which are bookmarks have
    // an action of openBookmarkMenuItem:.  Also, assume all items
    // with submenus are submenus of bookmarks.
    if (([item action] == @selector(openBookmarkMenuItem:)) ||
        ([item action] == @selector(openAllBookmarks:)) ||
        ([item action] == @selector(openAllBookmarksNewWindow:)) ||
        ([item action] == @selector(openAllBookmarksIncognitoWindow:)) ||
        [item hasSubmenu] ||
        [item isSeparatorItem]) {
      // This will eventually [obj release] all its kids, if it has
      // any.
      [menu removeItem:item];
    } else {
      // Leave it alone.
    }
  }
}

void BookmarkMenuBridge::AddNodeAsSubmenu(NSMenu* menu,
                                          const BookmarkNode* node,
                                          NSString* title) {
  NSMenuItem* items = [[[NSMenuItem alloc]
                               initWithTitle:title
                                      action:nil
                               keyEquivalent:@""] autorelease];
  [items setImage:folder_image_];
  [menu addItem:items];
  NSMenu* other_submenu = [[[NSMenu alloc] initWithTitle:title]
                            autorelease];
  [menu setSubmenu:other_submenu forItem:items];
  AddNodeToMenu(node, other_submenu);
}

// TODO(jrg): limit the number of bookmarks in the menubar?
void BookmarkMenuBridge::AddNodeToMenu(const BookmarkNode* node, NSMenu* menu) {
  int child_count = node->child_count();
  if (!child_count) {
    NSString* empty_string = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:empty_string
                                                   action:nil
                                            keyEquivalent:@""] autorelease];
    [menu addItem:item];
  } else for (int i = 0; i < child_count; i++) {
    const BookmarkNode* child = node->GetChild(i);
    NSString* title = [BookmarkMenuCocoaController menuTitleForNode:child];
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                   action:nil
                                            keyEquivalent:@""] autorelease];
    [menu addItem:item];
    bookmark_nodes_[child] = item;
    if (child->is_folder()) {
      [item setImage:folder_image_];
      NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menu setSubmenu:submenu forItem:item];
      AddNodeToMenu(child, submenu);  // recursive call
    } else {
      ConfigureMenuItem(child, item, false);
    }
  }

  // Add menus for 'Open All Bookmarks'.
  [menu addItem:[NSMenuItem separatorItem]];
  bool enabled = child_count != 0;
  AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL,
                IDS_BOOMARK_BAR_OPEN_ALL,
                node, menu, enabled);
  AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
                IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
                node, menu, enabled);
  AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
                IDS_BOOMARK_BAR_OPEN_INCOGNITO,
                node, menu, enabled);
}

void BookmarkMenuBridge::AddItemToMenu(int command_id,
                                       int message_id,
                                       const BookmarkNode* node,
                                       NSMenu* menu,
                                       bool enabled) {
  NSString* title = l10n_util::GetNSString(message_id);
  SEL action;
  if (!enabled) {
    // A nil action makes a menu item appear disabled. NSMenuItem setEnabled
    // will not reflect the disabled state until the item title is set again.
    action = nil;
  } else if (command_id == IDC_BOOKMARK_BAR_OPEN_ALL) {
    action = @selector(openAllBookmarks:);
  } else if (command_id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
    action = @selector(openAllBookmarksNewWindow:);
  } else {
    action = @selector(openAllBookmarksIncognitoWindow:);
  }
  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                 action:action
                                          keyEquivalent:@""] autorelease];
  [item setTarget:controller_];
  [item setTag:node->id()];
  [item setEnabled:enabled];
  [menu addItem:item];
}

void BookmarkMenuBridge::ConfigureMenuItem(const BookmarkNode* node,
                                           NSMenuItem* item,
                                           bool set_title) {
  if (set_title) {
    NSString* title = [BookmarkMenuCocoaController menuTitleForNode:node];
    [item setTitle:title];
  }
  [item setTarget:controller_];
  [item setAction:@selector(openBookmarkMenuItem:)];
  [item setTag:node->id()];
  if (node->is_url()) {
    // Add a tooltip
    std::string url_string = node->GetURL().possibly_invalid_spec();
    NSString* tooltip = [NSString stringWithFormat:@"%@\n%s",
                         base::SysUTF16ToNSString(node->GetTitle()),
                         url_string.c_str()];
    [item setToolTip:tooltip];
  }
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
    favicon = app::mac::GetCachedImageWithName(@"nav.pdf");
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
