// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_constants.h"

namespace webview {

// Attributes.
const char kAttributeAllowTransparency[] = "allowtransparency";
const char kAttributeAutoSize[] = "autosize";
const char kAttributeMaxHeight[] = "maxheight";
const char kAttributeMaxWidth[] = "maxwidth";
const char kAttributeMinHeight[] = "minheight";
const char kAttributeMinWidth[] = "minwidth";
const char kAttributeName[] = "name";
const char kAttributeSrc[] = "src";

// API namespace.
const char kAPINamespace[] = "webViewInternal";

// Events.
const char kEventClose[] = "webViewInternal.onClose";
const char kEventConsoleMessage[] = "webViewInternal.onConsoleMessage";
const char kEventContentLoad[] = "webViewInternal.onContentLoad";
const char kEventContextMenu[] = "chromeWebViewInternal.contextmenu";
const char kEventDialog[] = "webViewInternal.onDialog";
const char kEventExit[] = "webViewInternal.onExit";
const char kEventFindReply[] = "webViewInternal.onFindReply";
const char kEventFrameNameChanged[] = "webViewInternal.onFrameNameChanged";
const char kEventLoadAbort[] = "webViewInternal.onLoadAbort";
const char kEventLoadCommit[] = "webViewInternal.onLoadCommit";
const char kEventLoadProgress[] = "webViewInternal.onLoadProgress";
const char kEventLoadRedirect[] = "webViewInternal.onLoadRedirect";
const char kEventLoadStart[] = "webViewInternal.onLoadStart";
const char kEventLoadStop[] = "webViewInternal.onLoadStop";
const char kEventMessage[] = "webViewInternal.onMessage";
const char kEventNewWindow[] = "webViewInternal.onNewWindow";
const char kEventPermissionRequest[] = "webViewInternal.onPermissionRequest";
const char kEventPluginDestroyed[] = "webViewInternal.onPluginDestroyed";
const char kEventResponsive[] = "webViewInternal.onResponsive";
const char kEventSizeChanged[] = "webViewInternal.onSizeChanged";
const char kEventUnresponsive[] = "webViewInternal.onUnresponsive";
const char kEventZoomChange[] = "webViewInternal.onZoomChange";

// Event related constants.
const char kWebViewEventPrefix[] = "webViewInternal.";

// Parameters/properties on events.
const char kContextMenuItems[] = "items";
const char kDefaultPromptText[] = "defaultPromptText";
const char kFindSearchText[] = "searchText";
const char kFindFinalUpdate[] = "finalUpdate";
const char kInitialHeight[] = "initialHeight";
const char kInitialWidth[] = "initialWidth";
const char kLastUnlockedBySelf[] = "lastUnlockedBySelf";
const char kLevel[] = "level";
const char kLine[] = "line";
const char kMessage[] = "message";
const char kMessageText[] = "messageText";
const char kMessageType[] = "messageType";
const char kName[] = "name";
const char kNewHeight[] = "newHeight";
const char kNewURL[] = "newUrl";
const char kNewWidth[] = "newWidth";
const char kOldHeight[] = "oldHeight";
const char kOldURL[] = "oldUrl";
const char kPermission[] = "permission";
const char kPermissionTypeDialog[] = "dialog";
const char kPermissionTypeDownload[] = "download";
const char kPermissionTypeFileSystem[] = "filesystem";
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
const char kTargetURL[] = "targetUrl";
const char kWindowID[] = "windowId";
const char kWindowOpenDisposition[] = "windowOpenDisposition";
const char kOldZoomFactor[] = "oldZoomFactor";
const char kNewZoomFactor[] = "newZoomFactor";

// Internal parameters/properties on events.
const char kInternalBaseURLForDataURL[] = "baseUrlForDataUrl";
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
const char kMenuItemCommandId[] = "commandId";
const char kMenuItemLabel[] = "label";
const char kPersistPrefix[] = "persist:";
const char kStoragePartitionId[] = "storagePartitionId";
const unsigned int kMaxOutstandingPermissionRequests = 1024;
const int kInvalidPermissionRequestID = 0;

}  // namespace webview
