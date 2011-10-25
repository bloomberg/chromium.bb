// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the Bookmarks API.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_CONSTANTS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_CONSTANTS_H_
#pragma once

namespace bookmark_extension_api_constants {

// Keys.
extern const char kIdKey[];
extern const char kIndexKey[];
extern const char kParentIdKey[];
extern const char kOldIndexKey[];
extern const char kOldParentIdKey[];
extern const char kUrlKey[];
extern const char kTitleKey[];
extern const char kChildrenKey[];
extern const char kChildIdsKey[];
extern const char kRecursiveKey[];
extern const char kDateAddedKey[];
extern const char kDateFolderModifiedKey[];
// TODO(arv): Move bookmark manager related constants out of this file.
extern const char kSameProfileKey[];
extern const char kElementsKey[];

// Errors.
extern const char kNoNodeError[];
extern const char kNoParentError[];
extern const char kFolderNotEmptyError[];
extern const char kInvalidIdError[];
extern const char kInvalidIndexError[];
extern const char kInvalidUrlError[];
extern const char kModifySpecialError[];
extern const char kEditBookmarksDisabled[];

// Events.
extern const char kOnBookmarkCreated[];
extern const char kOnBookmarkRemoved[];
extern const char kOnBookmarkChanged[];
extern const char kOnBookmarkMoved[];
extern const char kOnBookmarkChildrenReordered[];
extern const char kOnBookmarkImportBegan[];
extern const char kOnBookmarkImportEnded[];
// TODO(arv): Move bookmark manager related constants out of this file.
extern const char kOnBookmarkDragEnter[];
extern const char kOnBookmarkDragLeave[];
extern const char kOnBookmarkDrop[];

};  // namespace bookmark_extension_api_constants

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_CONSTANTS_H_
