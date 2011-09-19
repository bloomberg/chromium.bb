// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

BookmarkEditor::EditDetails::EditDetails(Type node_type)
    : type(node_type) {
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::EditNode(
    const BookmarkNode* node) {
  EditDetails details(EXISTING_NODE);
  details.existing_node = node;
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddNodeInFolder(
    const BookmarkNode* parent_node) {
  EditDetails details(NEW_URL);
  details.parent_node = parent_node;
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddFolder(
    const BookmarkNode* parent_node) {
  EditDetails details(NEW_FOLDER);
  details.parent_node = parent_node;
  return details;
}

BookmarkEditor::EditDetails::~EditDetails() {
}

void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  // TODO(flackr): Implement NEW_FOLDER type in WebUI and remove the type check.
  if (ChromeWebUI::IsMoreWebUI() && (
        details.type == EditDetails::EXISTING_NODE ||
        details.type == EditDetails::NEW_URL)) {
    ShowWebUI(profile, details);
    return;
  }

  // Delegate to the platform native bookmark editor code.
  ShowNative(parent_window, profile, details.parent_node, details,
      configuration);
}
