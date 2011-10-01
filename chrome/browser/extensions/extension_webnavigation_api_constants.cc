// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"

namespace extension_webnavigation_api_constants {

const char kErrorKey[] = "error";
const char kErrorOccurredKey[] = "errorOccurred";
const char kFrameIdKey[] = "frameId";
const char kSourceTabIdKey[] = "sourceTabId";
const char kSourceFrameIdKey[] = "sourceFrameId";
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
const char kOnErrorOccurred[] = "webNavigation.onErrorOccurred";
const char kOnReferenceFragmentUpdated[] =
    "webNavigation.onReferenceFragmentUpdated";

}  // namespace extension_webnavigation_api_constants
