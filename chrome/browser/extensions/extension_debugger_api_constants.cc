// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_debugger_api_constants.h"

namespace extension_debugger_api_constants {

const char kCommandKey[] = "command";
const char kDataKey[] = "data";
const char kTabIdKey[] = "tabId";
const char kVersionIdKey[] = "requiredVersion";

const char kOnEvent[] = "debugger.onEvent";
const char kOnDetach[] = "debugger.onDetach";

const char kAlreadyAttachedError[] =
    "Another debugger is already attached to the tab with id: *.";
const char kAttachToWebUIError[] =
    "Can not attach to the page with the \"*://\" scheme.";
const char kDebuggerExecuteFailedError[] = "Execute debugger failed.";
const char kNoTabError[] = "No tab with given id *.";
const char kNotAttachedError[] =
    "Debugger is not attached to the tab with id: *.";
const char kPermissionError[] = "You do not have permission to debug pages.";
const char kProtocolVersionNotSupportedError[] =
    "Requested protocol version is not supported: *.";

}  // namespace extension_debugger_api_constants
