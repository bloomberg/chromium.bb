// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_sub_menu_model_gtk.h"

#include "base/stl_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"

using content::OpenURLParams;
using content::PageNavigator;

// Per chrome/app/chrome_command_ids.h, values < 4000 are for "dynamic menu
// items". We only use one command id for all the bookmarks, because we handle
// bookmark item activations directly. So we pick a suitably large random value
// and use that to avoid accidental conflicts with other dynamic items.
static const int kBookmarkItemCommandId = 1759;

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
  NavigateToMenuItem(index, ui::DispositionFromEventFlags(event_flags));
}

void BookmarkNodeMenuModel::PopulateMenu() {
  DCHECK(submenus_.empty());
  for (int i = 0; i < node_->child_count(); ++i) {
    const BookmarkNode* child = node_->GetChild(i);
    if (child->is_folder()) {
      AddSubMenuForNode(child);
    } else {
      // Ironically the label will end up getting converted back to UTF8 later.
      // We need to escape any Windows-style "&" characters since they will be
      // converted in MenuGtk outside of our control here.
      const string16 label = UTF8ToUTF16(ui::EscapeWindowsStyleAccelerators(
          bookmark_utils::BuildMenuLabelFor(child)));
      // No command id. We override ActivatedAt below to handle activations.
      AddItem(kBookmarkItemCommandId, label);
      const gfx::Image& node_icon = model_->GetFavicon(child);
      if (!node_icon.IsEmpty())
        SetIcon(GetItemCount() - 1, node_icon);
      // TODO(mdm): set up an observer to watch for icon load events and set
      // the icons in response.
    }
  }
}

void BookmarkNodeMenuModel::AddSubMenuForNode(const BookmarkNode* node) {
  DCHECK(node->is_folder());
  // Ironically the label will end up getting converted back to UTF8 later.
  // We need to escape any Windows-style "&" characters since they will be
  // converted in MenuGtk outside of our control here.
  const string16 label = UTF8ToUTF16(ui::EscapeWindowsStyleAccelerators(
      bookmark_utils::BuildMenuLabelFor(node)));
  // Don't pass in the delegate, if any. Bookmark submenus don't need one.
  BookmarkNodeMenuModel* submenu =
      new BookmarkNodeMenuModel(NULL, model_, node, page_navigator_);
  // No command id. Nothing happens if you click on the submenu itself.
  AddSubMenu(kBookmarkItemCommandId, label, submenu);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Image& folder_icon = rb.GetImageNamed(IDR_BOOKMARK_BAR_FOLDER);
  SetIcon(GetItemCount() - 1, folder_icon);
  submenus_.push_back(submenu);
}

void BookmarkNodeMenuModel::NavigateToMenuItem(
    int index,
    WindowOpenDisposition disposition) {
  const BookmarkNode* node = node_->GetChild(index);
  DCHECK(node);
  page_navigator_->OpenURL(OpenURLParams(
      node->url(), content::Referrer(), disposition,
      content::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));  // is_renderer_initiated
}

BookmarkSubMenuModel::BookmarkSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : BookmarkNodeMenuModel(delegate, NULL, NULL, browser),
      browser_(browser),
      fixed_items_(0),
      bookmark_end_(0),
      menu_(NULL),
      menu_showing_(false) {
}

BookmarkSubMenuModel::~BookmarkSubMenuModel() {
  if (model())
    model()->RemoveObserver(this);
}

void BookmarkSubMenuModel::Loaded(BookmarkModel* model, bool ids_reassigned) {
  // For now, just close the menu when the bookmarks are finished loading.
  // TODO(mdm): it would be slicker to just populate the menu while it's open.
  BookmarkModelChanged();
}

void BookmarkSubMenuModel::BookmarkModelChanged() {
  if (menu_showing_ && menu_)
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
  menu_showing_ = true;
  Clear();
  AddCheckItemWithStringId(IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR);
  AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_BOOKMARK_PAGE, IDS_BOOKMARK_STAR);
  fixed_items_ = bookmark_end_ = GetItemCount();
  if (!model()) {
    set_model(BookmarkModelFactory::GetForProfile(browser_->profile()));
    if (!model())
      return;
    model()->AddObserver(this);
  }
  // We can't do anything further if the model isn't loaded yet.
  if (!model()->IsLoaded())
    return;
  // The node count includes the node itself, so 1 means empty.
  if (model()->bookmark_bar_node()->GetTotalNodeCount() > 1) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    fixed_items_ = GetItemCount();
    if (!node())
      set_node(model()->bookmark_bar_node());
    // PopulateMenu() won't clear the items we added above.
    PopulateMenu();
  }
  bookmark_end_ = GetItemCount();

  // We want only one separator after the top-level bookmarks and before the
  // other node and/or mobile node.
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (model()->other_node()->GetTotalNodeCount() > 1)
    AddSubMenuForNode(model()->other_node());
  if (model()->mobile_node()->GetTotalNodeCount() > 1)
    AddSubMenuForNode(model()->mobile_node());
  RemoveTrailingSeparators();
}

void BookmarkSubMenuModel::MenuClosed() {
  menu_showing_ = false;
  BookmarkNodeMenuModel::MenuClosed();
}

void BookmarkSubMenuModel::ActivatedAt(int index) {
  // Because this is also overridden in BookmarkNodeMenuModel which doesn't know
  // we might be prepending items, we have to adjust the index for it.
  if (index >= fixed_items_ && index < bookmark_end_)
    BookmarkNodeMenuModel::ActivatedAt(index - fixed_items_);
  else
    SimpleMenuModel::ActivatedAt(index);
}

void BookmarkSubMenuModel::ActivatedAt(int index, int event_flags) {
  // Because this is also overridden in BookmarkNodeMenuModel which doesn't know
  // we might be prepending items, we have to adjust the index for it.
  if (index >= fixed_items_ && index < bookmark_end_)
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

// static
bool BookmarkSubMenuModel::IsBookmarkItemCommandId(int command_id) {
  return command_id == kBookmarkItemCommandId;
}
