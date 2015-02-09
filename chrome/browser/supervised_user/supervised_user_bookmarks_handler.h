// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BOOKMARKS_HANDLER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BOOKMARKS_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// This class converts bookmarks from supervised user settings into a tree of
// base::Values, for use in bookmarks::prefs::kSupervisedBookmarks.
class SupervisedUserBookmarksHandler {
 public:
  static scoped_ptr<base::ListValue> BuildBookmarksTree(
      const base::DictionaryValue& settings);

  // Public for testing only.
  struct Folder {
    Folder(int id, const std::string& name, int parent_id);

    int id;
    std::string name;
    int parent_id;
  };

  struct Link {
    Link(const std::string& url, const std::string& name, int parent_id);

    std::string url;
    std::string name;
    int parent_id;
  };

 private:
  friend class SupervisedUserBookmarksHandlerTest;

  SupervisedUserBookmarksHandler();
  ~SupervisedUserBookmarksHandler();

  void ParseSettings(const base::DictionaryValue& settings);
  void ParseFolders(const base::DictionaryValue& folders);
  void ParseLinks(const base::DictionaryValue& links);
  scoped_ptr<base::ListValue> BuildTree();
  void AddFoldersToTree();
  void AddLinksToTree();
  bool AddNodeToTree(int parent_id, scoped_ptr<base::DictionaryValue> node);

  const std::vector<Folder>& folders_for_testing() const { return folders_; }
  const std::vector<Link>& links_for_testing() const { return links_; }

  std::vector<Folder> folders_;
  std::vector<Link> links_;

  scoped_ptr<base::ListValue> root_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserBookmarksHandler);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BOOKMARKS_HANDLER_H_
