// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_drag_drop_views.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"
#include "ui/views/drag_utils.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkNodeData;

namespace chrome {

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view,
                   ui::DragDropTypes::DragEventSource source) {
  DCHECK(!nodes.empty());

  // Set up our OLE machinery.
  ui::OSExchangeData data;
  BookmarkNodeData drag_data(nodes);
  drag_data.Write(profile->GetPath(), &data);

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = base::MessageLoop::current()->IsNested();
  base::MessageLoop::current()->SetNestableTasksAllowed(true);

  int operation = ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  if (bookmarks::CanAllBeEditedByUser(model->client(), nodes))
    operation |= ui::DragDropTypes::DRAG_MOVE;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(view);

  if (widget) {
    widget->RunShellDrag(NULL, data, gfx::Point(), operation, source);
  } else {
    // We hit this case when we're using WebContentsViewWin or
    // WebContentsViewAura, instead of WebContentsViewViews.
    views::RunShellDrag(view, data, gfx::Point(), operation, source);
  }

  base::MessageLoop::current()->SetNestableTasksAllowed(was_nested);
}

int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const BookmarkNode* node) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);

  int move = ui::DragDropTypes::DRAG_MOVE;
  if (!prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) ||
      !model->client()->CanBeEditedByUser(node)) {
    move = ui::DragDropTypes::DRAG_NONE;
  }
  if (node->is_url())
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK | move;
  return ui::DragDropTypes::DRAG_COPY | move;
}

int GetPreferredBookmarkDropOperation(int source_operations, int operations) {
  int common_ops = (source_operations & operations);
  if (!common_ops)
    return ui::DragDropTypes::DRAG_NONE;
  if (ui::DragDropTypes::DRAG_COPY & common_ops)
    return ui::DragDropTypes::DRAG_COPY;
  if (ui::DragDropTypes::DRAG_LINK & common_ops)
    return ui::DragDropTypes::DRAG_LINK;
  if (ui::DragDropTypes::DRAG_MOVE & common_ops)
    return ui::DragDropTypes::DRAG_MOVE;
  return ui::DragDropTypes::DRAG_NONE;
}

int GetBookmarkDropOperation(Profile* profile,
                             const ui::DropTargetEvent& event,
                             const BookmarkNodeData& data,
                             const BookmarkNode* parent,
                             int index) {
  const base::FilePath& profile_path = profile->GetPath();

  if (data.IsFromProfilePath(profile_path) && data.size() > 1)
    // Currently only accept one dragged node at a time.
    return ui::DragDropTypes::DRAG_NONE;

  if (!IsValidBookmarkDropLocation(profile, data, parent, index))
    return ui::DragDropTypes::DRAG_NONE;

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  if (!model->client()->CanBeEditedByUser(parent))
    return ui::DragDropTypes::DRAG_NONE;

  const BookmarkNode* dragged_node =
      data.GetFirstNode(model, profile->GetPath());
  if (dragged_node) {
    // User is dragging from this profile.
    if (!model->client()->CanBeEditedByUser(dragged_node)) {
      // Do a copy instead of a move when dragging bookmarks that the user can't
      // modify.
      return ui::DragDropTypes::DRAG_COPY;
    }
    return ui::DragDropTypes::DRAG_MOVE;
  }

  // User is dragging from another app, copy.
  return GetPreferredBookmarkDropOperation(event.source_operations(),
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
}

bool IsValidBookmarkDropLocation(Profile* profile,
                                 const BookmarkNodeData& data,
                                 const BookmarkNode* drop_parent,
                                 int index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  if (!model->client()->CanBeEditedByUser(drop_parent))
    return false;

  const base::FilePath& profile_path = profile->GetPath();
  if (data.IsFromProfilePath(profile_path)) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(model, profile_path);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      const BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->parent()) ?
          drop_parent->GetIndexOf(nodes[i]) : -1;
      if (node_index != -1 && (index == node_index || index == node_index + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From another profile, always accept.
  return true;
}

}  // namespace chrome
