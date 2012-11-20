// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"

namespace debugger_api_constants {

const char kOnEvent[] = "debugger.onEvent";
const char kOnDetach[] = "debugger.onDetach";

const char kAlreadyAttachedError[] =
    "Another debugger is already attached to the tab with id: *.";
const char kAttachToWebUIError[] =
    "Can not attach to the page with the \"*://\" scheme.";
const char kNoTabError[] = "No tab with given id *.";
const char kNotAttachedError[] =
    "Debugger is not attached to the tab with id: *.";
const char kProtocolVersionNotSupportedError[] =
    "Requested protocol version is not supported: *.";

}  // namespace debugger_api_constants
