// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/browser_plugin/browser_plugin_constants.h"

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

// Internal method bindings.
const char kMethodInternalAttachWindowTo[] = "-internal-attachWindowTo";
const char kMethodInternalPersistObject[] = "-internal-persistObject";
const char kMethodInternalSetPermission[] = "-internal-setPermission";

// Attributes.
const char kAttributeAutoSize[] = "autosize";
const char kAttributeContentWindow[] = "contentWindow";
const char kAttributeMaxHeight[] = "maxheight";
const char kAttributeMaxWidth[] = "maxwidth";
const char kAttributeMinHeight[] = "minheight";
const char kAttributeMinWidth[] = "minwidth";
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
const char kEventNewWindow[] = "newwindow";
const char kEventRequestPermission[] = "permissionrequest";
const char kEventResponsive[] = "responsive";
const char kEventSizeChanged[] = "sizechanged";
const char kEventUnresponsive[] = "unresponsive";

// Parameters/properties on events.
const char kInitialHeight[] = "initialHeight";
const char kInitialWidth[] = "initialWidth";
const char kIsTopLevel[] = "isTopLevel";
const char kLastUnlockedBySelf[] = "lastUnlockedBySelf";
const char kName[] = "name";
const char kNewURL[] = "newUrl";
const char kNewHeight[] = "newHeight";
const char kNewWidth[] = "newWidth";
const char kOldURL[] = "oldUrl";
const char kOldHeight[] = "oldHeight";
const char kOldWidth[] = "oldWidth";
const char kPermission[] = "permission";
const char kPermissionTypeDownload[] = "download";
const char kPermissionTypeGeolocation[] = "geolocation";
const char kPermissionTypeMedia[] = "media";
const char kPermissionTypeNewWindow[] = "newwindow";
const char kPermissionTypePointerLock[] = "pointerLock";
const char kPersistPrefix[] = "persist:";
const char kProcessId[] = "processId";
const char kReason[] = "reason";
const char kRequestId[] = "requestId";
const char kRequestMethod[] = "requestMethod";
const char kTargetURL[] = "targetUrl";
const char kURL[] = "url";
const char kWindowID[] = "windowId";
const char kWindowOpenDisposition[] = "windowOpenDisposition";
const char kUserGesture[] = "userGesture";

// Error messages.
const char kErrorAlreadyNavigated[] =
    "The object has already navigated, so its partition cannot be changed.";
const char kErrorInvalidPartition[] =
    "Invalid partition attribute.";
const char kErrorCannotRemovePartition[] =
    "Cannot remove partition attribute after navigating.";

// Other.
const char kBrowserPluginGuestManagerKeyName[] = "browser_plugin_guest_manager";
const int kInstanceIDNone = 0;

}  // namespace browser_plugin

}  // namespace content
