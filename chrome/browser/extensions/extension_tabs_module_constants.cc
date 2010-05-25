// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace extension_tabs_module_constants {

const wchar_t kAllFramesKey[] = L"allFrames";
const wchar_t kCodeKey[] = L"code";
const wchar_t kFavIconUrlKey[] = L"favIconUrl";
const wchar_t kFileKey[] = L"file";
const wchar_t kFocusedKey[] = L"focused";
const wchar_t kFormatKey[] = L"format";
const wchar_t kFromIndexKey[] = L"fromIndex";
const wchar_t kHeightKey[] = L"height";
const wchar_t kIdKey[] = L"id";
const wchar_t kIncognitoKey[] = L"incognito";
const wchar_t kIndexKey[] = L"index";
const wchar_t kLeftKey[] = L"left";
const wchar_t kNewPositionKey[] = L"newPosition";
const wchar_t kNewWindowIdKey[] = L"newWindowId";
const wchar_t kOldPositionKey[] = L"oldPosition";
const wchar_t kOldWindowIdKey[] = L"oldWindowId";
const wchar_t kPopulateKey[] = L"populate";
const wchar_t kQualityKey[] = L"quality";
const wchar_t kSelectedKey[] = L"selected";
const wchar_t kStatusKey[] = L"status";
const wchar_t kTabIdKey[] = L"tabId";
const wchar_t kTabsKey[] = L"tabs";
const wchar_t kTabUrlKey[] = L"tabUrl";
const wchar_t kTitleKey[] = L"title";
const wchar_t kToIndexKey[] = L"toIndex";
const wchar_t kTopKey[] = L"top";
const wchar_t kUrlKey[] = L"url";
const wchar_t kWidthKey[] = L"width";
const wchar_t kWindowIdKey[] = L"windowId";
const wchar_t kWindowTypeKey[] = L"type";

const char kFormatValueJpeg[] = "jpeg";
const char kFormatValuePng[] = "png";
const char kMimeTypeJpeg[] = "image/jpg";
const char kMimeTypePng[] = "image/png";
const char kStatusValueComplete[] = "complete";
const char kStatusValueLoading[] = "loading";

// TODO(mpcomplete): should we expose more specific detail, like devtools, app
// panel, etc?
const char kWindowTypeValueNormal[] = "normal";
const char kWindowTypeValuePopup[] = "popup";
const char kWindowTypeValueApp[] = "app";

const char kCanOnlyMoveTabsWithinNormalWindowsError[] = "Tabs can only be "
    "moved to and from normal windows.";
const char kNoCurrentWindowError[] = "No current window";
const char kNoLastFocusedWindowError[] = "No last-focused window";
const char kWindowNotFoundError[] = "No window with id: *.";
const char kTabNotFoundError[] = "No tab with id: *.";
const char kNoSelectedTabError[] = "No selected tab";
const char kInvalidUrlError[] = "Invalid url: \"*\".";
const char kInternalVisibleTabCaptureError[] =
    "Internal error while trying to capture visible region of the current tab";
const char kNotImplementedError[] = "This call is not yet implemented";
const char kSupportedInWindowsOnlyError[] = "Supported in Windows only";

const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] = "Code and file should not be specified "
    "at the same time in the second argument.";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
const char kCannotUpdatePinnedTab[] = "Cannot update pinned tabs";
const char kCannotRemovePhantomTab[] = "Cannot remove phantom tabs";
const char kCannotDetermineLanguageOfUnloadedTab[] =
    "Cannot determine language: tab not loaded";

}  // namespace extension_tabs_module_constants
