// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_constants.h"

namespace content {

namespace browser_plugin {

// Method bindings.
const char kMethodBack[] = "back";
const char kMethodCanGoBack[] = "canGoBack";
const char kMethodCanGoForward[] = "canGoForward";
const char kMethodForward[] = "forward";
const char kMethodGetProcessId[] = "getProcessId";
const char kMethodGetRouteId[] = "getRouteId";
const char kMethodGo[] = "go";
const char kMethodReload[] = "reload";
const char kMethodStop[] = "stop";
const char kMethodTerminate[] = "terminate";

// Attributes.
const char kAttributeAutoSize[] = "autoSize";
const char kAttributeContentWindow[] = "contentWindow";
const char kAttributeMaxHeight[] = "maxHeight";
const char kAttributeMaxWidth[] = "maxWidth";
const char kAttributeMinHeight[] = "minHeight";
const char kAttributeMinWidth[] = "minWidth";
const char kAttributeName[] = "name";
const char kAttributePartition[] = "partition";
const char kAttributeSrc[] = "src";

// Events.
const char kEventExit[] = "exit";
const char kEventLoadAbort[] = "loadabort";
const char kEventLoadCommit[] = "loadcommit";
const char kEventLoadRedirect[] = "loadredirect";
const char kEventLoadStart[] = "loadstart";
const char kEventLoadStop[] = "loadstop";
const char kEventResponsive[] = "responsive";
const char kEventSizeChanged[] = "sizechanged";
const char kEventUnresponsive[] = "unresponsive";

// Parameters/properties on events.
const char kIsTopLevel[] = "isTopLevel";
const char kNewURL[] = "newUrl";
const char kNewHeight[] = "newHeight";
const char kNewWidth[] = "newWidth";
const char kOldURL[] = "oldUrl";
const char kOldHeight[] = "oldHeight";
const char kOldWidth[] = "oldWidth";
const char kPersistPrefix[] = "persist:";
const char kProcessId[] = "processId";
const char kReason[] = "reason";
const char kURL[] = "url";

// Error messages.
const char kErrorAlreadyNavigated[] =
    "The object has already navigated, so its partition cannot be changed.";
const char kErrorInvalidPartition[] =
    "Invalid partition attribute.";

// Other.
const int kInstanceIDNone = 0;

}  // namespace browser_plugin

}  // namespace content
