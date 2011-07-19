// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_editor.h"

#include "googleurl/src/gurl.h"

BookmarkEditor::EditDetails::EditDetails()
    : type(NEW_URL),
      existing_node(NULL) {
}

BookmarkEditor::EditDetails::EditDetails(const BookmarkNode* node)
    : type(EXISTING_NODE),
      existing_node(node) {
}

BookmarkEditor::EditDetails::~EditDetails() {
}
