// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_

namespace webview {

// Events.
extern const char kEventClose[];
extern const char kEventConsoleMessage[];
extern const char kEventContentLoad[];
extern const char kEventDialog[];
extern const char kEventExit[];
extern const char kEventFindReply[];
extern const char kEventLoadAbort[];
extern const char kEventLoadCommit[];
extern const char kEventLoadProgress[];
extern const char kEventLoadRedirect[];
extern const char kEventLoadStart[];
extern const char kEventLoadStop[];
extern const char kEventNewWindow[];
extern const char kEventPermissionRequest[];
extern const char kEventResponsive[];
extern const char kEventSizeChanged[];
extern const char kEventUnresponsive[];
extern const char kEventZoomChange[];

// Parameters/properties on events.
extern const char kFindSearchText[];
extern const char kFindFinalUpdate[];
extern const char kLevel[];
extern const char kLine[];
extern const char kMessage[];
extern const char kNewHeight[];
extern const char kNewURL[];
extern const char kNewWidth[];
extern const char kOldHeight[];
extern const char kOldURL[];
extern const char kPermission[];
extern const char kPermissionTypeDialog[];
extern const char kPermissionTypeDownload[];
extern const char kPermissionTypeGeolocation[];
extern const char kPermissionTypeLoadPlugin[];
extern const char kPermissionTypeMedia[];
extern const char kPermissionTypeNewWindow[];
extern const char kPermissionTypePointerLock[];
extern const char kOldWidth[];
extern const char kProcessId[];
extern const char kProgress[];
extern const char kReason[];
extern const char kRequestId[];
extern const char kSourceId[];
extern const char kOldZoomFactor[];
extern const char kNewZoomFactor[];

// Internal parameters/properties on events.
extern const char kInternalCurrentEntryIndex[];
extern const char kInternalEntryCount[];
extern const char kInternalProcessId[];

// Parameters to callback functions.
extern const char kFindNumberOfMatches[];
extern const char kFindActiveMatchOrdinal[];
extern const char kFindSelectionRect[];
extern const char kFindRectLeft[];
extern const char kFindRectTop[];
extern const char kFindRectWidth[];
extern const char kFindRectHeight[];
extern const char kFindCanceled[];
extern const char kFindDone[];

// Initialization parameters.
extern const char kParameterUserAgentOverride[];

// Miscellaneous.
extern const unsigned int kMaxOutstandingPermissionRequests;
extern const int kInvalidPermissionRequestID;

}  // namespace webview

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_
