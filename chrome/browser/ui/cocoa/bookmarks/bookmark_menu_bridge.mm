// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"

BookmarkMenuBridge::BookmarkMenuBridge(Profile* profile, NSMenu* menu)
    : menuIsValid_(false),
      profile_(profile),
      controller_([[BookmarkMenuCocoaController alloc] initWithBridge:this
                                                              andMenu:menu]) {
  if (GetBookmarkModel())
    ObserveBookmarkModel();
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  BookmarkModel* model = GetBookmarkModel();
  if (model)
    model->RemoveObserver(this);
  [controller_ release];
}

NSMenu* BookmarkMenuBridge::BookmarkMenu() {
  return [controller_ menu];
}

void BookmarkMenuBridge::Loaded(BookmarkModel* model, bool ids_reassigned) {
  InvalidateMenu();
}

void BookmarkMenuBridge::UpdateMenu(NSMenu* bookmark_menu) {
  UpdateMenuInternal(bookmark_menu, false);
}

void BookmarkMenuBridge::UpdateSubMenu(NSMenu* bookmark_menu) {
  UpdateMenuInternal(bookmark_menu, true);
}

void BookmarkMenuBridge::UpdateMenuInternal(NSMenu* bookmark_menu,
                                            bool is_submenu) {
  DCHECK(bookmark_menu);
  if (menuIsValid_)
    return;

  BookmarkModel* model = GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;

  if (!folder_image_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folder_image_.reset(
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER).CopyNSImage());
  }

  ClearBookmarkMenu(bookmark_menu);

  // Add bookmark bar items, if any.
  const BookmarkNode* barNode = model->bookmark_bar_node();
  CHECK(barNode);
  if (!barNode->empty()) {
    [bookmark_menu addItem:[NSMenuItem separatorItem]];
    AddNodeToMenu(barNode, bookmark_menu, !is_submenu);
  }

  // If the "Other Bookmarks" folder has any content, make a submenu for it and
  // fill it in.
  if (!model->other_node()->empty()) {
    [bookmark_menu addItem:[NSMenuItem separatorItem]];
    AddNodeAsSubmenu(bookmark_menu,
                     model->other_node(),
                     !is_submenu);
  }

  // If the "Mobile Bookmarks" folder has any content, make a submenu for it and
  // fill it in.
  if (!model->mobile_node()->empty()) {
    // Add a separator if we did not already add one due to a non-empty
    // "Other Bookmarks" folder.
    if (model->other_node()->empty())
      [bookmark_menu addItem:[NSMenuItem separatorItem]];

    AddNodeAsSubmenu(bookmark_menu,
                     model->mobile_node(),
                     !is_submenu);
  }

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

void BookmarkMenuBridge::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item, false);
}

void BookmarkMenuBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  InvalidateMenu();
}

void BookmarkMenuBridge::ResetMenu() {
  ClearBookmarkMenu(BookmarkMenu());
}

void BookmarkMenuBridge::BuildMenu() {
  UpdateMenu(BookmarkMenu());
}

// Watch for changes.
void BookmarkMenuBridge::ObserveBookmarkModel() {
  BookmarkModel* model = GetBookmarkModel();
  model->AddObserver(this);
  if (model->IsLoaded())
    Loaded(model, false);
}

BookmarkModel* BookmarkMenuBridge::GetBookmarkModel() {
  if (!profile_)
    return NULL;
  return BookmarkModelFactory::GetForProfile(profile_);
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
                                          bool add_extra_items) {
  NSString* title = SysUTF16ToNSString(node->GetTitle());
  NSMenuItem* items = [[[NSMenuItem alloc]
                            initWithTitle:title
                                   action:nil
                            keyEquivalent:@""] autorelease];
  [items setImage:folder_image_];
  [menu addItem:items];
  NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
  [menu setSubmenu:submenu forItem:items];
  AddNodeToMenu(node, submenu, add_extra_items);
}

// TODO(jrg): limit the number of bookmarks in the menubar?
void BookmarkMenuBridge::AddNodeToMenu(const BookmarkNode* node, NSMenu* menu,
                                       bool add_extra_items) {
  int child_count = node->child_count();
  if (!child_count) {
    NSString* empty_string = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
    NSMenuItem* item =
        [[[NSMenuItem alloc] initWithTitle:empty_string
                                    action:nil
                             keyEquivalent:@""] autorelease];
    [menu addItem:item];
  } else for (int i = 0; i < child_count; i++) {
    const BookmarkNode* child = node->GetChild(i);
    NSString* title = [BookmarkMenuCocoaController menuTitleForNode:child];
    NSMenuItem* item =
        [[[NSMenuItem alloc] initWithTitle:title
                                    action:nil
                             keyEquivalent:@""] autorelease];
    [menu addItem:item];
    bookmark_nodes_[child] = item;
    if (child->is_folder()) {
      [item setImage:folder_image_];
      NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menu setSubmenu:submenu forItem:item];
      AddNodeToMenu(child, submenu, add_extra_items);  // recursive call
    } else {
      ConfigureMenuItem(child, item, false);
    }
  }

  if (add_extra_items) {
    // Add menus for 'Open All Bookmarks'.
    [menu addItem:[NSMenuItem separatorItem]];
    bool enabled = child_count != 0;
    AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL,
                  IDS_BOOKMARK_BAR_OPEN_ALL,
                  node, menu, enabled);
    AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
                  IDS_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
                  node, menu, enabled);
    AddItemToMenu(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
                  IDS_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
                  node, menu, enabled);
  }
}

void BookmarkMenuBridge::AddItemToMenu(int command_id,
                                       int message_id,
                                       const BookmarkNode* node,
                                       NSMenu* menu,
                                       bool enabled) {
  NSString* title = l10n_util::GetNSStringWithFixup(message_id);
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
  if (set_title)
    [item setTitle:[BookmarkMenuCocoaController menuTitleForNode:node]];
  [item setTarget:controller_];
  [item setAction:@selector(openBookmarkMenuItem:)];
  [item setTag:node->id()];
  if (node->is_url())
    [item setToolTip:[BookmarkMenuCocoaController tooltipForNode:node]];
  // Check to see if we have a favicon.
  NSImage* favicon = nil;
  BookmarkModel* model = GetBookmarkModel();
  if (model) {
    const gfx::Image& image = model->GetFavicon(node);
    if (!image.IsEmpty())
      favicon = image.ToNSImage();
  }
  // If we do not have a loaded favicon, use the default site image instead.
  if (!favicon) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    favicon = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
  }
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
