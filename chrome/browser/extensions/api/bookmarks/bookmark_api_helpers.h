// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/extensions/api/bookmarks.h"

class BookmarkModel;
class BookmarkNode;

// Helper functions.
namespace extensions {
namespace bookmark_api_helpers {

// The returned value is owned by the caller.
api::bookmarks::BookmarkTreeNode* GetBookmarkTreeNode(
    const BookmarkNode* node,
    bool recurse,
    bool only_folders);

// TODO(mwrosen): Remove this function once chrome.bookmarkManagerPrivate is
// refactored to use the JSON schema compiler.
base::DictionaryValue* GetNodeDictionary(const BookmarkNode* node,
                                         bool recurse,
                                         bool only_folders);

// Add a JSON representation of |node| to the JSON |nodes|.
void AddNode(const BookmarkNode* node,
             std::vector<linked_ptr<api::bookmarks::BookmarkTreeNode> >* nodes,
             bool recurse);

void AddNodeFoldersOnly(const BookmarkNode* node,
                        std::vector<linked_ptr<
                            api::bookmarks::BookmarkTreeNode> >* nodes,
                        bool recurse);

// TODO(mwrosen): Remove this function once chrome.bookmarkManagerPrivate is
// refactored to use the JSON schema compiler.
void AddNode(const BookmarkNode* node,
             base::ListValue* list,
             bool recurse);

// TODO(mwrosen): Remove this function once chrome.bookmarkManagerPrivate is
// refactored to use the JSON schema compiler.
void AddNodeFoldersOnly(const BookmarkNode* node,
                        base::ListValue* list,
                        bool recurse);

bool RemoveNode(BookmarkModel* model,
                int64 id,
                bool recursive,
                std::string* error);

}  // namespace bookmark_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_
