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

// Parameters/properties on events.
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
extern const char kPermissionTypeMedia[];
extern const char kPermissionTypeNewWindow[];
extern const char kPermissionTypePointerLock[];
extern const char kOldWidth[];
extern const char kProcessId[];
extern const char kProgress[];
extern const char kReason[];
extern const char kRequestId[];
extern const char kSourceId[];

// Internal parameters/properties on events.
extern const char kInternalCurrentEntryIndex[];
extern const char kInternalEntryCount[];
extern const char kInternalProcessId[];

// Initialization parameters.
extern const char kParameterUserAgentOverride[];

}  // namespace webview

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_

