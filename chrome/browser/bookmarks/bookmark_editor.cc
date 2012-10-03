// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_editor.h"

#include "grit/generated_resources.h"

BookmarkEditor::EditDetails::EditDetails(Type node_type)
    : type(node_type), existing_node(NULL), parent_node(NULL), index(-1) {
}

BookmarkNode::Type BookmarkEditor::EditDetails::GetNodeType() const {
  BookmarkNode::Type node_type = BookmarkNode::URL;
  switch (type) {
    case EXISTING_NODE:
      node_type = existing_node->type();
      break;
    case NEW_URL:
      node_type = BookmarkNode::URL;
      break;
    case NEW_FOLDER:
      node_type = BookmarkNode::FOLDER;
      break;
    default:
      NOTREACHED();
  }
  return node_type;
}

int BookmarkEditor::EditDetails::GetWindowTitleId() const {
  int dialog_title = IDS_BOOKMARK_EDITOR_TITLE;
  switch (type) {
    case EditDetails::EXISTING_NODE:
    case EditDetails::NEW_URL:
      dialog_title = (type == EditDetails::EXISTING_NODE &&
                      existing_node->type() == BookmarkNode::FOLDER) ?
          IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE :
          IDS_BOOKMARK_EDITOR_TITLE;
      break;
    case EditDetails::NEW_FOLDER:
      dialog_title = urls.empty() ?
          IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW :
          IDS_BOOKMARK_MANAGER_BOOKMARK_ALL_TABS;
      break;
    default:
      NOTREACHED();
  }
  return dialog_title;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::EditNode(
    const BookmarkNode* node) {
  EditDetails details(EXISTING_NODE);
  details.existing_node = node;
  if (node)
    details.parent_node = node->parent();
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddNodeInFolder(
    const BookmarkNode* parent_node,
    int index,
    const GURL& url,
    const string16& title) {
  EditDetails details(NEW_URL);
  details.parent_node = parent_node;
  details.index = index;
  details.url = url;
  details.title = title;
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddFolder(
    const BookmarkNode* parent_node,
    int index) {
  EditDetails details(NEW_FOLDER);
  details.parent_node = parent_node;
  details.index = index;
  return details;
}

BookmarkEditor::EditDetails::~EditDetails() {
}
