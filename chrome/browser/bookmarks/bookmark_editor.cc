// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_input_window_dialog_controller.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

BookmarkEditor::EditDetails::EditDetails(Type node_type)
    : type(node_type), existing_node(NULL), parent_node(NULL) {
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
    int index) {
  EditDetails details(NEW_URL);
  details.parent_node = parent_node;
  details.index = index;
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

void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  if ((details.type == EditDetails::EXISTING_NODE &&
       details.existing_node->is_folder()) ||
      (details.type == EditDetails::NEW_FOLDER &&
       details.urls.empty())) {
    BookmarkInputWindowDialogController::Show(profile, parent_window, details);
    return;
  }

  // Delegate to the platform native bookmark editor code.
  ShowNative(parent_window, profile, details.parent_node, details,
      configuration);
}

void BookmarkEditor::ShowBookmarkAllTabsDialog(Browser* browser) {
  Profile* profile = browser->profile();
  BookmarkModel* model = profile->GetBookmarkModel();
  DCHECK(model && model->IsLoaded());

  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(model->GetParentForNewNodes(), -1);
  bookmark_utils::GetURLsForOpenTabs(browser, &(details.urls));
  DCHECK(!details.urls.empty());

  BookmarkEditor::Show(browser->window()->GetNativeHandle(),
                       profile, details, BookmarkEditor::SHOW_TREE);
}

