// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the Bookmarks API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_CONSTANTS_H_

namespace extension_bookmarks_module_constants {

// Keys.
extern const wchar_t kIdKey[];
extern const wchar_t kIndexKey[];
extern const wchar_t kParentIdKey[];
extern const wchar_t kOldIndexKey[];
extern const wchar_t kOldParentIdKey[];
extern const wchar_t kUrlKey[];
extern const wchar_t kTitleKey[];
extern const wchar_t kChildrenKey[];
extern const wchar_t kChildIdsKey[];
extern const wchar_t kRecursiveKey[];
extern const wchar_t kDateAddedKey[];
extern const wchar_t kDateGroupModifiedKey[];
// TODO(arv): Move bookmark manager related constants out of this file.
extern const wchar_t kSameProfileKey[];
extern const wchar_t kElementsKey[];

// Errors.
extern const char kNoNodeError[];
extern const char kNoParentError[];
extern const char kFolderNotEmptyError[];
extern const char kInvalidIdError[];
extern const char kInvalidIndexError[];
extern const char kInvalidUrlError[];
extern const char kModifySpecialError[];

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

};  // namespace extension_bookmarks_module_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_CONSTANTS_H_
