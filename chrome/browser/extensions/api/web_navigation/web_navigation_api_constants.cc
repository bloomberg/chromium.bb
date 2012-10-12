// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"

namespace extensions {

namespace web_navigation_api_constants {

const char kErrorKey[] = "error";
const char kFrameIdKey[] = "frameId";
const char kParentFrameIdKey[] = "parentFrameId";
const char kProcessIdKey[] = "processId";
const char kReplacedTabIdKey[] = "replacedTabId";
const char kSourceFrameIdKey[] = "sourceFrameId";
const char kSourceProcessIdKey[] = "sourceProcessId";
const char kSourceTabIdKey[] = "sourceTabId";
const char kTabIdKey[] = "tabId";
const char kTimeStampKey[] = "timeStamp";
const char kTransitionTypeKey[] = "transitionType";
const char kTransitionQualifiersKey[] = "transitionQualifiers";
const char kUrlKey[] = "url";

const char kOnBeforeNavigate[] = "webNavigation.onBeforeNavigate";
const char kOnCommitted[] = "webNavigation.onCommitted";
const char kOnCompleted[] = "webNavigation.onCompleted";
const char kOnCreatedNavigationTarget[] =
    "webNavigation.onCreatedNavigationTarget";
const char kOnDOMContentLoaded[] =
    "webNavigation.onDOMContentLoaded";
const char kOnHistoryStateUpdated[] = "webNavigation.onHistoryStateUpdated";
const char kOnErrorOccurred[] = "webNavigation.onErrorOccurred";
const char kOnReferenceFragmentUpdated[] =
    "webNavigation.onReferenceFragmentUpdated";
const char kOnTabReplaced[] = "webNavigation.onTabReplaced";

}  // namespace web_navigation_api_constants

}  // namespace extensions
