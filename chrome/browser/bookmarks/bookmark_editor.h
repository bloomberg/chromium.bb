// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
#pragma once

#include <utility>
#include <vector>

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

class BookmarkNode;
class GURL;
class Profile;

// Small, cross platform interface that shows the correct platform specific
// bookmark editor dialog.
class BookmarkEditor {
 public:
  // An enumeration of the possible configurations offered.
  enum Configuration {
    SHOW_TREE,
    NO_TREE
  };

  // Describes what the user is editing.
  struct EditDetails {
    enum Type {
      // The user is editing an existing node in the model. The node the user
      // is editing is set in |existing_node|.
      EXISTING_NODE,

      // A new bookmark should be created if the user accepts the edit.
      // |existing_node| is null in this case.
      NEW_URL,

      // A new folder bookmark should be created if the user accepts the edit.
      // The contents of the folder should be that of |urls|.
      // |existing_node| is null in this case.
      NEW_FOLDER
    };

    EditDetails();
    explicit EditDetails(const BookmarkNode* node);
    ~EditDetails();

    // See description of enum value for details.
    Type type;

    // If type == EXISTING_NODE this gives the existing node.
    const BookmarkNode* existing_node;

    // If type == NEW_FOLDER, this is the urls/title pairs to add to the
    // folder.
    std::vector<std::pair<GURL, string16> > urls;
  };

  // Shows the bookmark editor. The bookmark editor allows editing an
  // existing node or creating a new bookmark node (as determined by
  // |details.type|). If |configuration| is SHOW_TREE, a tree is shown allowing
  // the user to choose the parent of the node.
  // |parent| gives the initial parent to select in the tree for the node.
  // |parent| is only used if |details.existing_node| is null.
  static void Show(gfx::NativeWindow parent_window,
                   Profile* profile,
                   const BookmarkNode* parent,
                   const EditDetails& details,
                   Configuration configuration);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
