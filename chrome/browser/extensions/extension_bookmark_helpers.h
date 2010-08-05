// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_HELPERS_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class BookmarkModel;
class BookmarkNode;
class DictionaryValue;
class ListValue;

// Helper functions.
namespace extension_bookmark_helpers {

DictionaryValue* GetNodeDictionary(const BookmarkNode* node,
                                   bool recurse,
                                   bool only_folders);

// Add a JSON representation of |node| to the JSON |list|.
void AddNode(const BookmarkNode* node, ListValue* list, bool recurse);

void AddNodeFoldersOnly(const BookmarkNode* node,
                        ListValue* list,
                        bool recurse);

bool RemoveNode(BookmarkModel* model,
                int64 id,
                bool recursive,
                std::string* error);

}  // namespace extension_bookmark_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_HELPERS_H_
