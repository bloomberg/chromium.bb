// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_undo_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/core/browser/bookmark_model.h"
#include "components/bookmarks/core/browser/bookmark_node_data.h"
#include "components/bookmarks/core/browser/bookmark_utils.h"
#include "components/bookmarks/core/browser/scoped_group_bookmark_actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace chrome {

int DropBookmarks(Profile* profile,
                  const BookmarkNodeData& data,
                  const BookmarkNode* parent_node,
                  int index) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
#if !defined(OS_ANDROID)
  ScopedGroupBookmarkActions group_drops(model);
#endif
  if (data.IsFromProfilePath(profile->GetPath())) {
    const std::vector<const BookmarkNode*> dragged_nodes =
        data.GetNodes(model, profile->GetPath());
    if (!dragged_nodes.empty()) {
      // Drag from same profile. Move nodes.
      for (size_t i = 0; i < dragged_nodes.size(); ++i) {
        model->Move(dragged_nodes[i], parent_node, index);
        index = parent_node->GetIndexOf(dragged_nodes[i]) + 1;
      }
      return ui::DragDropTypes::DRAG_MOVE;
    }
    return ui::DragDropTypes::DRAG_NONE;
  }
  // Dropping a folder from different profile. Always accept.
  bookmark_utils::CloneBookmarkNode(model, data.elements, parent_node,
                                    index, true);
  return ui::DragDropTypes::DRAG_COPY;
}

}  // namespace chrome
