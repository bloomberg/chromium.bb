// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"

namespace extension_bookmarks_module_constants {

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
// TODO(arv): Move bookmark manager related constants out of this file.
const char kSameProfileKey[] = "sameProfile";
const char kElementsKey[] = "elements";

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
// TODO(arv): Move bookmark manager related constants out of this file.
const char kOnBookmarkDragEnter[] =
    "experimental.bookmarkManager.onDragEnter";
const char kOnBookmarkDragLeave[] =
    "experimental.bookmarkManager.onDragLeave";
const char kOnBookmarkDrop[] =
    "experimental.bookmarkManager.onDrop";
}  // namespace extension_bookmarks_module_constants
