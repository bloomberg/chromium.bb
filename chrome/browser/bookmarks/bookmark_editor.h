// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class Browser;

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
  class EditDetails {
   public:
    // Returns an EditDetails instance for the user editing the given bookmark.
    static EditDetails EditNode(const BookmarkNode* node);

    // Returns an EditDetails instance for the user adding a bookmark within
    // a given parent node with a specified index.
    static EditDetails AddNodeInFolder(const BookmarkNode* parent_node,
                                       int index);

    // Returns an EditDetails instance for the user adding a folder within a
    // given parent node with a specified index.
    static EditDetails AddFolder(const BookmarkNode* parent_node,
                                 int index);

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

    ~EditDetails();

    // See description of enum value for details.
    const Type type;

    // If type == EXISTING_NODE this gives the existing node.
    const BookmarkNode* existing_node;

    // If type == NEW_URL or type == NEW_FOLDER this gives the parent node
    // to place the new node in.
    const BookmarkNode* parent_node;

    // If type == NEW_URL or type == NEW_FOLDER this gives the index to insert
    // the new node at.
    int index;

    // If type == NEW_FOLDER, this is the urls/title pairs to add to the
    // folder.
    std::vector<std::pair<GURL, string16> > urls;

   private:
    explicit EditDetails(Type node_type);
  };

  // Shows the bookmark editor. If --use-more-webui is enabled use the bookmark
  // manager to add or edit bookmarks. The bookmark editor allows editing an
  // existing node or creating a new bookmark node (as determined by
  // |details.type|). If |configuration| is SHOW_TREE, a tree is shown allowing
  // the user to choose the parent of the node.
  // |parent| gives the initial parent to select in the tree for the node.
  // |parent| is only used if |details.existing_node| is null.
  static void Show(gfx::NativeWindow parent_window,
                   Profile* profile,
                   const EditDetails& details,
                   Configuration configuration);

  // Shows the bookmark all tabs dialog.
  static void ShowBookmarkAllTabsDialog(Browser* browser);

#if !defined(USE_AURA)
  // Shows the native bookmark all tabs dialog. This is delegated from
  // ShowBookmarkAllTabsDialog() when use_aura is not set.
  static void ShowNativeBookmarkAllTabsDialog(Browser* browser);
#endif  // !defined(USE_AURA)

 private:
  // Shows the native bookmark editor.
  // TODO(flackr): Remove parent argument.
  static void ShowNative(gfx::NativeWindow parent_window,
                         Profile* profile,
                         const BookmarkNode* parent,
                         const EditDetails& details,
                         Configuration configuration);

  // Shows the WebUI bookmark editor.
  static void ShowWebUI(Profile* profile,
                        const EditDetails& details);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
