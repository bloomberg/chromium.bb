// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_
#define  CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_

namespace content {

namespace browser_plugin {

// Method bindings.
extern const char kMethodBack[];
extern const char kMethodCanGoBack[];
extern const char kMethodCanGoForward[];
extern const char kMethodForward[];
extern const char kMethodGetProcessId[];
extern const char kMethodGetRouteId[];
extern const char kMethodGo[];
extern const char kMethodReload[];
extern const char kMethodStop[];
extern const char kMethodTerminate[];

// Internal method bindings.
extern const char kMethodInternalPersistObject[];
extern const char kMethodInternalSetPermission[];

// Attributes.
extern const char kAttributeAutoSize[];
extern const char kAttributeContentWindow[];
extern const char kAttributeMaxHeight[];
extern const char kAttributeMaxWidth[];
extern const char kAttributeMinHeight[];
extern const char kAttributeMinWidth[];
extern const char kAttributeName[];
extern const char kAttributePartition[];
extern const char kAttributeSrc[];

// Events.
extern const char kEventExit[];
extern const char kEventLoadAbort[];
extern const char kEventLoadCommit[];
extern const char kEventLoadRedirect[];
extern const char kEventLoadStart[];
extern const char kEventLoadStop[];
extern const char kEventRequestPermission[];
extern const char kEventResponsive[];
extern const char kEventSizeChanged[];
extern const char kEventUnresponsive[];

// Parameters/properties on events.
extern const char kIsTopLevel[];
extern const char kNewURL[];
extern const char kNewHeight[];
extern const char kNewWidth[];
extern const char kOldURL[];
extern const char kOldHeight[];
extern const char kOldWidth[];
extern const char kPermission[];
extern const char kPermissionTypeMedia[];
extern const char kPersistPrefix[];
extern const char kProcessId[];
extern const char kReason[];
extern const char kRequestId[];
extern const char kURL[];

// Error messages.
extern const char kErrorAlreadyNavigated[];
extern const char kErrorInvalidPartition[];
extern const char kErrorCannotRemovePartition[];

// Other.
extern const int kInstanceIDNone;

}  // namespace browser_plugin

}  // namespace content

#endif  // CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_
