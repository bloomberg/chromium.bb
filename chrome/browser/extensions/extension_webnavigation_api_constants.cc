// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"

namespace extension_webnavigation_api_constants {

const char kErrorKey[] = "error";
const char kFrameIdKey[] = "frameId";
const char kRequestIdKey[] = "requestId";
const char kSourceTabIdKey[] = "sourceTabId";
const char kSourceUrlKey[] = "sourceUrl";
const char kTabIdKey[] = "tabId";
const char kTimeStampKey[] = "timeStamp";
const char kTransitionTypeKey[] = "transitionType";
const char kTransitionQualifiersKey[] = "transitionQualifiers";
const char kUrlKey[] = "url";

const char kOnBeforeNavigate[] = "experimental.webNavigation.onBeforeNavigate";
const char kOnBeforeRetarget[] = "experimental.webNavigation.onBeforeRetarget";
const char kOnCommitted[] = "experimental.webNavigation.onCommitted";
const char kOnCompleted[] = "experimental.webNavigation.onCompleted";
const char kOnDOMContentLoaded[] =
    "experimental.webNavigation.onDOMContentLoaded";
const char kOnErrorOccurred[] = "experimental.webNavigation.onErrorOccurred";

}  // namespace extension_webnavigation_api_constants
