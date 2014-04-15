// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/webview_constants.h"

namespace webview {

// Events.
const char kEventClose[] = "webview.onClose";
const char kEventConsoleMessage[] = "webview.onConsoleMessage";
const char kEventContentLoad[] = "webview.onContentLoad";
const char kEventDialog[] = "webview.onDialog";
const char kEventExit[] = "webview.onExit";
const char kEventFindReply[] = "webview.onFindReply";
const char kEventLoadAbort[] = "webview.onLoadAbort";
const char kEventLoadCommit[] = "webview.onLoadCommit";
const char kEventLoadProgress[] = "webview.onLoadProgress";
const char kEventLoadRedirect[] = "webview.onLoadRedirect";
const char kEventLoadStart[] = "webview.onLoadStart";
const char kEventLoadStop[] = "webview.onLoadStop";
const char kEventNewWindow[] = "webview.onNewWindow";
const char kEventPermissionRequest[] = "webview.onPermissionRequest";
const char kEventResponsive[] = "webview.onResponsive";
const char kEventSizeChanged[] = "webview.onSizeChanged";
const char kEventUnresponsive[] = "webview.onUnresponsive";
const char kEventZoomChange[] = "webview.onZoomChange";

// Parameters/properties on events.
const char kFindSearchText[] = "searchText";
const char kFindFinalUpdate[] = "finalUpdate";
const char kLevel[] = "level";
const char kLine[] = "line";
const char kMessage[] = "message";
const char kNewHeight[] = "newHeight";
const char kNewURL[] = "newUrl";
const char kNewWidth[] = "newWidth";
const char kOldHeight[] = "oldHeight";
const char kOldURL[] = "oldUrl";
const char kPermission[] = "permission";
const char kPermissionTypeDialog[] = "dialog";
const char kPermissionTypeDownload[] = "download";
const char kPermissionTypeGeolocation[] = "geolocation";
const char kPermissionTypeLoadPlugin[] = "loadplugin";
const char kPermissionTypeMedia[] = "media";
const char kPermissionTypeNewWindow[] = "newwindow";
const char kPermissionTypePointerLock[] = "pointerLock";
const char kOldWidth[] = "oldWidth";
const char kProcessId[] = "processId";
const char kProgress[] = "progress";
const char kReason[] = "reason";
const char kRequestId[] = "requestId";
const char kSourceId[] = "sourceId";
const char kOldZoomFactor[] = "oldZoomFactor";
const char kNewZoomFactor[] = "newZoomFactor";

// Internal parameters/properties on events.
const char kInternalCurrentEntryIndex[] = "currentEntryIndex";
const char kInternalEntryCount[] = "entryCount";
const char kInternalProcessId[] = "processId";

// Parameters to callback functions.
const char kFindNumberOfMatches[] = "numberOfMatches";
const char kFindActiveMatchOrdinal[] = "activeMatchOrdinal";
const char kFindSelectionRect[] = "selectionRect";
const char kFindRectLeft[] = "left";
const char kFindRectTop[] = "top";
const char kFindRectWidth[] = "width";
const char kFindRectHeight[] = "height";
const char kFindCanceled[] = "canceled";

// Initialization parameters.
const char kParameterUserAgentOverride[] = "userAgentOverride";

// Miscellaneous.
const unsigned int kMaxOutstandingPermissionRequests = 1024;
const int kInvalidPermissionRequestID = 0;

}  // namespace webview
