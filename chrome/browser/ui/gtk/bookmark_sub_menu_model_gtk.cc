// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmark_sub_menu_model_gtk.h"

#include "base/stl_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BookmarkNodeMenuModel::BookmarkNodeMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BookmarkModel* model,
    const BookmarkNode* node,
    PageNavigator* page_navigator)
    : SimpleMenuModel(delegate),
      model_(model),
      node_(node),
      page_navigator_(page_navigator) {
  DCHECK(page_navigator_);
}

BookmarkNodeMenuModel::~BookmarkNodeMenuModel() {
  Clear();
}

void BookmarkNodeMenuModel::Clear() {
  SimpleMenuModel::Clear();
  STLDeleteElements(&submenus_);
}

void BookmarkNodeMenuModel::MenuWillShow() {
  Clear();
  PopulateMenu();
}

void BookmarkNodeMenuModel::MenuClosed() {
  Clear();
}

void BookmarkNodeMenuModel::ActivatedAt(int index) {
  NavigateToMenuItem(index, CURRENT_TAB);
}

void BookmarkNodeMenuModel::ActivatedAt(int index, int event_flags) {
  NavigateToMenuItem(index, browser::DispositionFromEventFlags(event_flags));
}

void BookmarkNodeMenuModel::PopulateMenu() {
  DCHECK(submenus_.empty());
  for (int i = 0; i < node_->child_count(); ++i) {
    const BookmarkNode* child = node_->GetChild(i);
    // Ironically the label will end up getting converted back to UTF8 later...
    const string16 label =
        UTF8ToUTF16(bookmark_utils::BuildMenuLabelFor(child));
    if (child->is_folder()) {
      // Don't pass in the delegate, if any. Bookmark submenus don't need one.
      BookmarkNodeMenuModel* submenu =
          new BookmarkNodeMenuModel(NULL, model_, child, page_navigator_);
      // No command id. Nothing happens if you click on the submenu itself.
      AddSubMenu(0, label, submenu);
      submenus_.push_back(submenu);
    } else {
      // No command id. We override ActivatedAt below to handle activations.
      AddItem(0, label);
      const SkBitmap& node_icon = model_->GetFavicon(child);
      if (node_icon.width() > 0)
        SetIcon(GetItemCount() - 1, node_icon);
      // TODO(mdm): set up an observer to watch for icon load events and set
      // the icons in response.
    }
  }
}

void BookmarkNodeMenuModel::NavigateToMenuItem(
    int index,
    WindowOpenDisposition disposition) {
  const BookmarkNode* node = node_->GetChild(index);
  DCHECK(node);
  page_navigator_->OpenURL(OpenURLParams(
      node->url(), GURL(), disposition,
      content::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));  // is_renderer_initiated
}

BookmarkSubMenuModel::BookmarkSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : BookmarkNodeMenuModel(delegate, NULL, NULL, browser),
      browser_(browser),
      fixed_items_(0),
      menu_(NULL) {
}

BookmarkSubMenuModel::~BookmarkSubMenuModel() {
  if (model())
    model()->RemoveObserver(this);
}

void BookmarkSubMenuModel::BookmarkModelChanged() {
  if (menu_)
    menu_->Cancel();
}

void BookmarkSubMenuModel::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  set_model(NULL);
  // All our submenus will still have pointers to the model, but this call
  // should force the menu to close, which will cause them to be deleted.
  BookmarkModelChanged();
}

void BookmarkSubMenuModel::MenuWillShow() {
  Clear();
  AddCheckItemWithStringId(IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR);
  AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
  if (!model()) {
    set_model(browser_->profile()->GetBookmarkModel());
    if (model())
      model()->AddObserver(this);
  }
  if (!model() ||
      (model()->bookmark_bar_node()->GetTotalNodeCount() == 0 &&
       model()->other_node()->GetTotalNodeCount() == 0)) {
    fixed_items_ = GetItemCount();
    return;
  }
  AddSeparator();
  fixed_items_ = GetItemCount();
  if (!node())
    set_node(model()->bookmark_bar_node());
  // PopulateMenu() won't clear the items we added above.
  PopulateMenu();
  // TODO(mdm): add a separator and the "other bookmarks" node here.
}

void BookmarkSubMenuModel::ActivatedAt(int index) {
  // Because this is also overridden in BookmarkNodeMenuModel which doesn't know
  // we might be prepending items, we have to adjust the index for it.
  if (index >= fixed_items_)
    BookmarkNodeMenuModel::ActivatedAt(index - fixed_items_);
  else
    SimpleMenuModel::ActivatedAt(index);
}

void BookmarkSubMenuModel::ActivatedAt(int index, int event_flags) {
  // Because this is also overridden in BookmarkNodeMenuModel which doesn't know
  // we might be prepending items, we have to adjust the index for it.
  if (index >= fixed_items_)
    BookmarkNodeMenuModel::ActivatedAt(index - fixed_items_, event_flags);
  else
    SimpleMenuModel::ActivatedAt(index, event_flags);
}

bool BookmarkSubMenuModel::IsEnabledAt(int index) const {
  // We don't want the delegate interfering with bookmark items.
  return index >= fixed_items_ || SimpleMenuModel::IsEnabledAt(index);
}

bool BookmarkSubMenuModel::IsVisibleAt(int index) const {
  // We don't want the delegate interfering with bookmark items.
  return index >= fixed_items_ || SimpleMenuModel::IsVisibleAt(index);
}
