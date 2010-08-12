// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_navigation_api_constants.h"

namespace extension_web_navigation_api_constants {

const wchar_t kErrorKey[] = L"error";
const wchar_t kFrameIdKey[] = L"frameId";
const wchar_t kRequestIdKey[] = L"requestId";
const wchar_t kSourceTabIdKey[] = L"sourceTabId";
const wchar_t kSourceUrlKey[] = L"sourceUrl";
const wchar_t kTabIdKey[] = L"tabId";
const wchar_t kTargetUrlKey[] = L"targetUrl";
const wchar_t kTimeStampKey[] = L"timeStamp";
const wchar_t kTransitionTypeKey[] = L"transitionType";
const wchar_t kUrlKey[] = L"url";

const char kOnBeforeNavigate[] = "experimental.webNavigation.onBeforeNavigate";
const char kOnBeforeRetarget[] = "experimental.webNavigation.onBeforeRetarget";
const char kOnCommitted[] = "experimental.webNavigation.onCommitted";
const char kOnCompleted[] = "experimental.webNavigation.onCompleted";
const char kOnDOMContentLoaded[] =
    "experimental.webNavigation.onDOMContentLoaded";
const char kOnErrorOccurred[] = "experimental.webNavigation.onErrorOccurred";

}  // namespace extension_web_navigation_api_constants
