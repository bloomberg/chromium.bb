// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_

#include <string>
#include <utility>
#include <vector>

#include "app/gfx/native_widget_types.h"

class BookmarkNode;
class GURL;
class Profile;

// Small, cross platform interface that shows the correct platform specific
// bookmark editor dialog.
class BookmarkEditor {
 public:
  // Handler is notified when the BookmarkEditor creates a new bookmark.
  // Handler is owned by the BookmarkEditor and deleted when it is deleted.
  class Handler {
   public:
    virtual ~Handler() {}
    virtual void NodeCreated(const BookmarkNode* new_node) = 0;
  };

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

    EditDetails() : type(NEW_URL), existing_node(NULL) {}

    explicit EditDetails(const BookmarkNode* node)
        : type(EXISTING_NODE),
          existing_node(node) {
    }

    // See description of enum value for details.
    Type type;

    // If type == EXISTING_NODE this gives the existing node.
    const BookmarkNode* existing_node;

    // If type == NEW_FOLDER, this is the urls/title pairs to add to the
    // folder.
    std::vector<std::pair<GURL, std::wstring> > urls;
  };

  // Shows the bookmark editor. The bookmark editor allows editing an
  // existing node or creating a new bookmark node (as determined by
  // |details.type|). If |configuration| is SHOW_TREE, a tree is shown allowing
  // the user to choose the parent of the node.
  // |parent| gives the initial parent to select in the tree for the node.
  // |parent| is only used if |details.existing_node| is null.
  // BookmarkEditor takes ownership of |handler| and deletes it when done.
  // |handler| may be null. See description of Handler for details.
  static void Show(gfx::NativeWindow parent_window,
                   Profile* profile,
                   const BookmarkNode* parent,
                   const EditDetails& details,
                   Configuration configuration,
                   Handler* handler);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
