// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"

namespace extensions {
namespace bookmark_api_constants {

const char kIdKey[] = "id";
const char kIndexKey[] = "index";
const char kParentIdKey[] = "parentId";
const char kOldIndexKey[] = "oldIndex";
const char kOldParentIdKey[] = "oldParentId";
const char kUrlKey[] = "url";
const char kTitleKey[] = "title";
const char kChildrenKey[] = "children";
const char kChildIdsKey[] = "childIds";
const char kRecursiveKey[] = "recursive";
const char kDateAddedKey[] = "dateAdded";
const char kDateFolderModifiedKey[] = "dateGroupModified";

const char kNoNodeError[] = "Can't find bookmark for id.";
const char kNoParentError[] = "Can't find parent bookmark for id.";
const char kFolderNotEmptyError[] =
    "Can't remove non-empty folder (use recursive to force).";
const char kInvalidIdError[] = "Bookmark id is invalid.";
const char kInvalidIndexError[] = "Index out of bounds.";
const char kInvalidUrlError[] = "Invalid URL.";
const char kModifySpecialError[] = "Can't modify the root bookmark folders.";
const char kEditBookmarksDisabled[] = "Bookmark editing is disabled.";

const char kOnBookmarkCreated[] = "bookmarks.onCreated";
const char kOnBookmarkRemoved[] = "bookmarks.onRemoved";
const char kOnBookmarkChanged[] = "bookmarks.onChanged";
const char kOnBookmarkMoved[] = "bookmarks.onMoved";
const char kOnBookmarkChildrenReordered[] = "bookmarks.onChildrenReordered";
const char kOnBookmarkImportBegan[] = "bookmarks.onImportBegan";
const char kOnBookmarkImportEnded[] = "bookmarks.onImportEnded";

}  // namespace bookmark_api_constants
}  // namespace extensions
