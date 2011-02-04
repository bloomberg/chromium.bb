// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace extension_tabs_module_constants {

const char kAllFramesKey[] = "allFrames";
const char kCodeKey[] = "code";
const char kFavIconUrlKey[] = "favIconUrl";
const char kFileKey[] = "file";
const char kFocusedKey[] = "focused";
const char kFormatKey[] = "format";
const char kFromIndexKey[] = "fromIndex";
const char kHeightKey[] = "height";
const char kIdKey[] = "id";
const char kIncognitoKey[] = "incognito";
const char kIndexKey[] = "index";
const char kLeftKey[] = "left";
const char kNewPositionKey[] = "newPosition";
const char kNewWindowIdKey[] = "newWindowId";
const char kOldPositionKey[] = "oldPosition";
const char kOldWindowIdKey[] = "oldWindowId";
const char kPinnedKey[] = "pinned";
const char kPopulateKey[] = "populate";
const char kQualityKey[] = "quality";
const char kSelectedKey[] = "selected";
const char kStatusKey[] = "status";
const char kTabIdKey[] = "tabId";
const char kTabsKey[] = "tabs";
const char kTabUrlKey[] = "tabUrl";
const char kTitleKey[] = "title";
const char kToIndexKey[] = "toIndex";
const char kTopKey[] = "top";
const char kUrlKey[] = "url";
const char kWindowClosing[] = "isWindowClosing";
const char kWidthKey[] = "width";
const char kWindowIdKey[] = "windowId";
const char kWindowTypeKey[] = "type";

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
const char kCanOnlyMoveTabsWithinSameProfileError[] = "Tabs can only be moved "
    "between windows in the same profile.";
const char kNoCurrentWindowError[] = "No current window";
const char kNoLastFocusedWindowError[] = "No last-focused window";
const char kWindowNotFoundError[] = "No window with id: *.";
const char kTabNotFoundError[] = "No tab with id: *.";
const char kTabStripNotEditableError[] =
    "Tabs cannot be edited right now (user may be dragging a tab).";
const char kNoSelectedTabError[] = "No selected tab";
const char kIncognitoModeIsDisabled[] = "Incognito mode is disabled.";
const char kInvalidUrlError[] = "Invalid url: \"*\".";
const char kInternalVisibleTabCaptureError[] =
    "Internal error while trying to capture visible region of the current tab";
const char kNotImplementedError[] = "This call is not yet implemented";
const char kSupportedInWindowsOnlyError[] = "Supported in Windows only";

const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] = "Code and file should not be specified "
    "at the same time in the second argument.";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
const char kCannotDetermineLanguageOfUnloadedTab[] =
    "Cannot determine language: tab not loaded";

}  // namespace extension_tabs_module_constants
