// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_menu_controller_views.h"

#include "app/l10n_util.h"
#include "app/os_exchange_data.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/common/page_transition_types.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/menu_button.h"

using views::MenuItemView;

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               gfx::NativeWindow parent,
                                               const BookmarkNode* node,
                                               int start_child_index,
                                               bool show_other_folder)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      parent_(parent),
      node_(node),
      menu_(NULL),
      observer_(NULL),
      for_drop_(false),
      show_other_folder_(show_other_folder),
      bookmark_bar_(NULL),
      next_menu_id_(1) {
  menu_ = CreateMenu(node, start_child_index);
}

void BookmarkMenuController::RunMenuAt(BookmarkBarView* bookmark_bar,
                                       bool for_drop) {
  bookmark_bar_ = bookmark_bar;
  views::MenuButton* menu_button = bookmark_bar_->GetMenuButtonForNode(node_);
  DCHECK(menu_button);
  MenuItemView::AnchorPosition anchor;
  int start_index;
  bookmark_bar_->GetAnchorPositionAndStartIndexForButton(
      menu_button, &anchor, &start_index);
  RunMenuAt(menu_button, anchor, for_drop);
}

void BookmarkMenuController::RunMenuAt(
    views::MenuButton* button,
    MenuItemView::AnchorPosition position,
    bool for_drop) {
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(button, &screen_loc);
  // Subtract 1 from the height to make the popup flush with the button border.
  gfx::Rect bounds(screen_loc.x(), screen_loc.y(), button->width(),
                   button->height() - 1);
  for_drop_ = for_drop;
  profile_->GetBookmarkModel()->AddObserver(this);
  // The constructor creates the initial menu and that is all we should have
  // at this point.
  DCHECK(node_to_menu_map_.size() == 1);
  if (for_drop) {
    menu_->RunMenuForDropAt(parent_, bounds, position);
  } else {
    menu_->RunMenuAt(parent_, button, bounds, position, false);
    delete this;
  }
}

void BookmarkMenuController::Cancel() {
  menu_->Cancel();
}

bool BookmarkMenuController::IsTriggerableEvent(const views::MouseEvent& e) {
  return event_utils::IsPossibleDispositionEvent(e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  DCHECK(page_navigator_);
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  GURL url = menu_id_to_node_map_[id]->GetURL();
  page_navigator_->OpenURL(
      url, GURL(), event_utils::DispositionFromEventFlags(mouse_event_flags),
      PageTransition::AUTO_BOOKMARK);
}

bool BookmarkMenuController::GetDropFormats(
      MenuItemView* menu,
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  *formats = OSExchangeData::URL;
  custom_formats->insert(BookmarkDragData::GetBookmarkCustomFormat());
  return true;
}

bool BookmarkMenuController::AreDropTypesRequired(MenuItemView* menu) {
  return true;
}

bool BookmarkMenuController::CanDrop(MenuItemView* menu,
                                     const OSExchangeData& data) {
  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.

  if (!drop_data_.Read(data) || drop_data_.elements.size() != 1)
    return false;

  if (drop_data_.has_single_url())
    return true;

  const BookmarkNode* drag_node = drop_data_.GetFirstNode(profile_);
  if (!drag_node) {
    // Dragging a group from another profile, always accept.
    return true;
  }

  // Drag originated from same profile and is not a URL. Only accept it if
  // the dragged node is not a parent of the node menu represents.
  const BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  while (drop_node && drop_node != drag_node)
    drop_node = drop_node->GetParent();
  return (drop_node == NULL);
}

int BookmarkMenuController::GetDropOperation(
    MenuItemView* item,
    const views::DropTargetEvent& event,
    DropPosition* position) {
  // Should only get here if we have drop data.
  DCHECK(drop_data_.is_valid());

  const BookmarkNode* node = menu_id_to_node_map_[item->GetCommand()];
  const BookmarkNode* drop_parent = node->GetParent();
  int index_to_drop_at = drop_parent->IndexOfChild(node);
  if (*position == DROP_AFTER) {
    if (node == profile_->GetBookmarkModel()->other_node()) {
      // The other folder is shown after all bookmarks on the bookmark bar.
      // Dropping after the other folder makes no sense.
      *position = DROP_NONE;
    }
    index_to_drop_at++;
  } else if (*position == DROP_ON) {
    drop_parent = node;
    index_to_drop_at = node->GetChildCount();
  }
  DCHECK(drop_parent);
  return bookmark_utils::BookmarkDropOperation(
      profile_, event, drop_data_, drop_parent, index_to_drop_at);
}

int BookmarkMenuController::OnPerformDrop(MenuItemView* menu,
                                          DropPosition position,
                                          const views::DropTargetEvent& event) {
  const BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model);
  const BookmarkNode* drop_parent = drop_node->GetParent();
  DCHECK(drop_parent);
  int index_to_drop_at = drop_parent->IndexOfChild(drop_node);
  if (position == DROP_AFTER) {
    index_to_drop_at++;
  } else if (position == DROP_ON) {
    DCHECK(drop_node->is_folder());
    drop_parent = drop_node;
    index_to_drop_at = drop_node->GetChildCount();
  }

  int result = bookmark_utils::PerformBookmarkDrop(
      profile_, drop_data_, drop_parent, index_to_drop_at);
  if (for_drop_)
    delete this;
  return result;
}

bool BookmarkMenuController::ShowContextMenu(MenuItemView* source,
                                             int id,
                                             int x,
                                             int y,
                                             bool is_mouse_gesture) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(menu_id_to_node_map_[id]);
  context_menu_.reset(
      new BookmarkContextMenu(parent_,
                              profile_,
                              page_navigator_,
                              nodes[0]->GetParent(),
                              nodes,
                              BookmarkContextMenuController::BOOKMARK_BAR));
  context_menu_->RunMenuAt(gfx::Point(x, y));
  context_menu_.reset(NULL);
  return true;
}

void BookmarkMenuController::DropMenuClosed(MenuItemView* menu) {
  delete this;
}

bool BookmarkMenuController::CanDrag(MenuItemView* menu) {
  const BookmarkNode* node = menu_id_to_node_map_[menu->GetCommand()];
  // Don't let users drag the other folder.
  return node->GetParent() != profile_->GetBookmarkModel()->root_node();
}

void BookmarkMenuController::WriteDragData(MenuItemView* sender,
                                           OSExchangeData* data) {
  DCHECK(sender && data);

  UserMetrics::RecordAction(L"BookmarkBar_DragFromFolder", profile_);

  BookmarkDragData drag_data(menu_id_to_node_map_[sender->GetCommand()]);
  drag_data.Write(profile_, data);
}

int BookmarkMenuController::GetDragOperations(MenuItemView* sender) {
  return bookmark_utils::BookmarkDragOperation(
      menu_id_to_node_map_[sender->GetCommand()]);
}

views::MenuItemView* BookmarkMenuController::GetSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    views::MenuItemView::AnchorPosition* anchor,
    bool* has_mnemonics,
    views::MenuButton** button) {
  if (show_other_folder_ || !bookmark_bar_ || for_drop_)
    return NULL;
  gfx::Point bookmark_bar_loc(screen_point);
  views::View::ConvertPointToView(NULL, bookmark_bar_, &bookmark_bar_loc);
  int start_index;
  const BookmarkNode* node =
      bookmark_bar_->GetNodeForButtonAt(bookmark_bar_loc, &start_index);
  if (!node || !node->is_folder())
    return NULL;

  MenuItemView* alt_menu = node_to_menu_map_[node];
  if (!alt_menu)
    alt_menu = CreateMenu(node, start_index);

  if (alt_menu)
    menu_ = alt_menu;

  *button = bookmark_bar_->GetMenuButtonForNode(node);
  bookmark_bar_->GetAnchorPositionAndStartIndexForButton(
      *button, anchor, &start_index);
  *has_mnemonics = false;
  return alt_menu;
}

void BookmarkMenuController::BookmarkModelChanged() {
  menu_->Cancel();
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  if (node_to_menu_id_map_.find(node) != node_to_menu_id_map_.end())
    menu_->SetIcon(model->GetFavIcon(node), node_to_menu_id_map_[node]);
}

MenuItemView* BookmarkMenuController::CreateMenu(const BookmarkNode* parent,
                                                 int start_child_index) {
  MenuItemView* menu = new MenuItemView(this);
  menu->SetCommand(next_menu_id_++);
  menu_id_to_node_map_[menu->GetCommand()] = parent;
  menu->set_has_icons(true);
  BuildMenu(parent, start_child_index, menu, &next_menu_id_);
  if (show_other_folder_)
    BuildOtherFolderMenu(menu, &next_menu_id_);
  node_to_menu_map_[parent] = menu;
  return menu;
}

void BookmarkMenuController::BuildOtherFolderMenu(
  MenuItemView* menu, int* next_menu_id) {
  const BookmarkNode* other_folder = profile_->GetBookmarkModel()->other_node();
  int id = *next_menu_id;
  (*next_menu_id)++;
  SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  MenuItemView* submenu = menu->AppendSubMenuWithIcon(
      id, l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_BOOKMARKED), *folder_icon);
  BuildMenu(other_folder, 0, submenu, next_menu_id);
  menu_id_to_node_map_[id] = other_folder;
}

void BookmarkMenuController::BuildMenu(const BookmarkNode* parent,
                                       int start_child_index,
                                       MenuItemView* menu,
                                       int* next_menu_id) {
  DCHECK(!parent->GetChildCount() ||
         start_child_index < parent->GetChildCount());
  for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
    const BookmarkNode* node = parent->GetChild(i);
    int id = *next_menu_id;

    (*next_menu_id)++;
    if (node->is_url()) {
      SkBitmap icon = profile_->GetBookmarkModel()->GetFavIcon(node);
      if (icon.width() == 0) {
        icon = *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_DEFAULT_FAVICON);
      }
      menu->AppendMenuItemWithIcon(id, node->GetTitle(), icon);
      node_to_menu_id_map_[node] = id;
    } else if (node->is_folder()) {
      SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
      MenuItemView* submenu =
          menu->AppendSubMenuWithIcon(id, node->GetTitle(), *folder_icon);
      BuildMenu(node, 0, submenu, next_menu_id);
    } else {
      NOTREACHED();
    }
    menu_id_to_node_map_[id] = node;
  }
}

BookmarkMenuController::~BookmarkMenuController() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
  if (observer_)
    observer_->BookmarkMenuDeleted(this);
  STLDeleteValues(&node_to_menu_map_);
}
