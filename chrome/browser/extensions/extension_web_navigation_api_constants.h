// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebNavigation API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_NAVIGATION_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_NAVIGATION_API_CONSTANTS_H_
#pragma once

namespace extension_web_navigation_api_constants {

// Keys.
extern const wchar_t kErrorKey[];
extern const wchar_t kFrameIdKey[];
extern const wchar_t kRequestIdKey[];
extern const wchar_t kSourceTabIdKey[];
extern const wchar_t kSourceUrlKey[];
extern const wchar_t kTabIdKey[];
extern const wchar_t kTargetUrlKey[];
extern const wchar_t kTimeStampKey[];
extern const wchar_t kTransitionTypeKey[];
extern const wchar_t kUrlKey[];

// Events.
extern const char kOnBeforeNavigate[];
extern const char kOnBeforeRetarget[];
extern const char kOnCommitted[];
extern const char kOnCompleted[];
extern const char kOnDOMContentLoaded[];
extern const char kOnErrorOccurred[];

}  // namespace extension_web_navigation_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_NAVIGATION_API_CONSTANTS_H_
