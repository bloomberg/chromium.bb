// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_HELPERS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_HELPERS_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class BookmarkModel;
class BookmarkNode;

namespace base {
class DictionaryValue;
class ListValue;
}

// Helper functions.
namespace bookmark_extension_helpers {

// The returned value is owned by the caller.
base::DictionaryValue* GetNodeDictionary(const BookmarkNode* node,
                                         bool recurse,
                                         bool only_folders);

// Add a JSON representation of |node| to the JSON |list|.
void AddNode(const BookmarkNode* node, base::ListValue* list, bool recurse);

void AddNodeFoldersOnly(const BookmarkNode* node,
                        base::ListValue* list,
                        bool recurse);

bool RemoveNode(BookmarkModel* model,
                int64 id,
                bool recursive,
                std::string* error);

}  // namespace bookmark_extension_helpers

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_HELPERS_H_
