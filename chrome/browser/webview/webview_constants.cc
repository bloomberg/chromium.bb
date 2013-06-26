// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webview/webview_constants.h"

namespace webview {

// Events.
const char kEventLoadCommit[] = "webview.onLoadCommit";

// Parameters/properties on events.
const char kIsTopLevel[] = "isTopLevel";
const char kUrl[] = "url";

// Internal parameters/properties on events.
const char kInternalCurrentEntryIndex[] = "currentEntryIndex";
const char kInternalEntryCount[] = "entryCount";

// Attributes.
const char kAttributeApi[] = "api";

}  // namespace webview
