// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/common/page_transition_types.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/menu_button.h"

using views::MenuItemView;

// Max width of a menu. There does not appear to be an OS value for this, yet
// both IE and FF restrict the max width of a menu.
static const int kMaxMenuWidth = 400;

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               gfx::NativeWindow parent,
                                               const BookmarkNode* node,
                                               int start_child_index)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      parent_(parent),
      node_(node),
      menu_(NULL),
      observer_(NULL),
      for_drop_(false),
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

std::wstring BookmarkMenuController::GetTooltipText(
    int id, const gfx::Point& screen_loc) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());

  const BookmarkNode* node = menu_id_to_node_map_[id];
  if (node->type() == BookmarkNode::URL)
    return BookmarkBarView::CreateToolTipForURLAndTitle(
        screen_loc, node->GetURL(), UTF16ToWide(node->GetTitle()), profile_);
  return std::wstring();
}

bool BookmarkMenuController::IsTriggerableEvent(const views::MouseEvent& e) {
  return event_utils::IsPossibleDispositionEvent(e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  DCHECK(page_navigator_);
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());

  const BookmarkNode* node = menu_id_to_node_map_[id];
  std::vector<const BookmarkNode*> selection;
  selection.push_back(node);

  WindowOpenDisposition initial_disposition =
      event_utils::DispositionFromEventFlags(mouse_event_flags);

  bookmark_utils::OpenAll(parent_, profile_, page_navigator_, selection,
                          initial_disposition);
}

bool BookmarkMenuController::GetDropFormats(
      MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  *formats = ui::OSExchangeData::URL;
  custom_formats->insert(BookmarkNodeData::GetBookmarkCustomFormat());
  return true;
}

bool BookmarkMenuController::AreDropTypesRequired(MenuItemView* menu) {
  return true;
}

bool BookmarkMenuController::CanDrop(MenuItemView* menu,
                                     const ui::OSExchangeData& data) {
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
                                             const gfx::Point& p,
                                             bool is_mouse_gesture) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(menu_id_to_node_map_[id]);
  context_menu_.reset(
      new BookmarkContextMenu(
          parent_,
          profile_,
          page_navigator_,
          nodes[0]->GetParent(),
          nodes));
  context_menu_->set_observer(this);
  context_menu_->RunMenuAt(p);
  context_menu_.reset(NULL);
  return true;
}

void BookmarkMenuController::DropMenuClosed(MenuItemView* menu) {
  delete this;
}

bool BookmarkMenuController::CanDrag(MenuItemView* menu) {
  return true;
}

void BookmarkMenuController::WriteDragData(MenuItemView* sender,
                                           ui::OSExchangeData* data) {
  DCHECK(sender && data);

  UserMetrics::RecordAction(UserMetricsAction("BookmarkBar_DragFromFolder"),
                            profile_);

  BookmarkNodeData drag_data(menu_id_to_node_map_[sender->GetCommand()]);
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
  if (!bookmark_bar_ || for_drop_)
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

  menu_ = alt_menu;

  *button = bookmark_bar_->GetMenuButtonForNode(node);
  bookmark_bar_->GetAnchorPositionAndStartIndexForButton(
      *button, anchor, &start_index);
  *has_mnemonics = false;
  return alt_menu;
}

int BookmarkMenuController::GetMaxWidthForMenu() {
  return kMaxMenuWidth;
}

void BookmarkMenuController::BookmarkModelChanged() {
  menu_->Cancel();
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  NodeToMenuIDMap::iterator menu_pair = node_to_menu_id_map_.find(node);
  if (menu_pair == node_to_menu_id_map_.end())
    return;  // We're not showing a menu item for the node.

  // Iterate through the menus looking for the menu containing node.
  for (NodeToMenuMap::iterator i = node_to_menu_map_.begin();
       i != node_to_menu_map_.end(); ++i) {
    MenuItemView* menu_item = i->second->GetMenuItemByID(menu_pair->second);
    if (menu_item) {
      menu_item->SetIcon(model->GetFavIcon(node));
      return;
    }
  }
}

void BookmarkMenuController::WillRemoveBookmarks(
    const std::vector<const BookmarkNode*>& bookmarks) {
  std::set<MenuItemView*> removed_menus;

  WillRemoveBookmarksImpl(bookmarks, &removed_menus);

  STLDeleteElements(&removed_menus);
}

void BookmarkMenuController::DidRemoveBookmarks() {
  profile_->GetBookmarkModel()->AddObserver(this);
}

MenuItemView* BookmarkMenuController::CreateMenu(const BookmarkNode* parent,
                                                 int start_child_index) {
  MenuItemView* menu = new MenuItemView(this);
  menu->SetCommand(next_menu_id_++);
  menu_id_to_node_map_[menu->GetCommand()] = parent;
  menu->set_has_icons(true);
  BuildMenu(parent, start_child_index, menu, &next_menu_id_);
  node_to_menu_map_[parent] = menu;
  return menu;
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
      menu->AppendMenuItemWithIcon(id, UTF16ToWide(node->GetTitle()), icon);
      node_to_menu_id_map_[node] = id;
    } else if (node->is_folder()) {
      SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
      MenuItemView* submenu = menu->AppendSubMenuWithIcon(id,
          UTF16ToWide(node->GetTitle()), *folder_icon);
      node_to_menu_id_map_[node] = id;
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

MenuItemView* BookmarkMenuController::GetMenuByID(int id) {
  for (NodeToMenuMap::const_iterator i = node_to_menu_map_.begin();
       i != node_to_menu_map_.end(); ++i) {
    MenuItemView* menu = i->second->GetMenuItemByID(id);
    if (menu)
      return menu;
  }
  return NULL;
}

void BookmarkMenuController::WillRemoveBookmarksImpl(
      const std::vector<const BookmarkNode*>& bookmarks,
      std::set<views::MenuItemView*>* removed_menus) {
  // Remove the observer so that when the remove happens we don't prematurely
  // cancel the menu.
  profile_->GetBookmarkModel()->RemoveObserver(this);

  // Remove the menu items.
  std::set<MenuItemView*> changed_parent_menus;
  for (std::vector<const BookmarkNode*>::const_iterator i = bookmarks.begin();
       i != bookmarks.end(); ++i) {
    NodeToMenuIDMap::iterator node_to_menu = node_to_menu_id_map_.find(*i);
    if (node_to_menu != node_to_menu_id_map_.end()) {
      MenuItemView* menu = GetMenuByID(node_to_menu->second);
      DCHECK(menu);  // If there an entry in node_to_menu_id_map_, there should
                     // be a menu.
      removed_menus->insert(menu);
      changed_parent_menus.insert(menu->GetParentMenuItem());
      menu->parent()->RemoveChildView(menu);
      node_to_menu_id_map_.erase(node_to_menu);
    }
  }

  // All the bookmarks in |bookmarks| should have the same parent. It's possible
  // to support different parents, but this would need to prune any nodes whose
  // parent has been removed. As all nodes currently have the same parent, there
  // is the DCHECK.
  DCHECK(changed_parent_menus.size() <= 1);

  for (std::set<MenuItemView*>::const_iterator i = changed_parent_menus.begin();
       i != changed_parent_menus.end(); ++i) {
    (*i)->ChildrenChanged();
  }

  // Remove any descendants of the removed nodes in node_to_menu_id_map_.
  for (NodeToMenuIDMap::iterator i = node_to_menu_id_map_.begin();
       i != node_to_menu_id_map_.end(); ) {
    bool ancestor_removed = false;
    for (std::vector<const BookmarkNode*>::const_iterator j = bookmarks.begin();
         j != bookmarks.end(); ++j) {
      if (i->first->HasAncestor(*j)) {
        ancestor_removed = true;
        break;
      }
    }
    if (ancestor_removed) {
      node_to_menu_id_map_.erase(i++);
    } else {
      ++i;
    }
  }
}
