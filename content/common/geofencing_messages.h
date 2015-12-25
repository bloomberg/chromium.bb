// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC message for geofencing

#include <stdint.h>

#include "content/common/geofencing_types.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

// Singly-included section for typedefs.
#ifndef CONTENT_COMMON_GEOFENCING_MESSAGES_H_
#define CONTENT_COMMON_GEOFENCING_MESSAGES_H_

typedef std::map<std::string, blink::WebCircularGeofencingRegion>
    GeofencingRegistrations;

#endif  // CONTENT_COMMON_GEOFENCING_MESSAGES_H_

// Multiply-included message file, hence no include guard.
#define IPC_MESSAGE_START GeofencingMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::GeofencingStatus,
                          content::GEOFENCING_STATUS_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(content::GeofencingMockState,
                          content::GeofencingMockState::LAST)

IPC_STRUCT_TRAITS_BEGIN(blink::WebCircularGeofencingRegion)
  IPC_STRUCT_TRAITS_MEMBER(latitude)
  IPC_STRUCT_TRAITS_MEMBER(longitude)
  IPC_STRUCT_TRAITS_MEMBER(radius)
IPC_STRUCT_TRAITS_END()

// Messages sent from the child process to the browser.
IPC_MESSAGE_CONTROL5(GeofencingHostMsg_RegisterRegion,
                     int /* thread_id */,
                     int /* request_id */,
                     std::string /* region_id */,
                     blink::WebCircularGeofencingRegion /* region */,
                     int64_t /* serviceworker_registration_id */)

IPC_MESSAGE_CONTROL4(GeofencingHostMsg_UnregisterRegion,
                     int /* thread_id */,
                     int /* request_id */,
                     std::string /* region_id */,
                     int64_t /* serviceworker_registration_id */)

IPC_MESSAGE_CONTROL3(GeofencingHostMsg_GetRegisteredRegions,
                     int /* thread_id */,
                     int /* request_id */,
                     int64_t /* serviceworker_registration_id */)

IPC_MESSAGE_CONTROL1(GeofencingHostMsg_SetMockProvider,
                     content::GeofencingMockState /* mock_state */)

IPC_MESSAGE_CONTROL2(GeofencingHostMsg_SetMockPosition,
                     double /* latitude */,
                     double /* longitude */)

// Messages sent from the browser to the child process.

// Reply in response to GeofencingHostMsg_RegisterRegion
IPC_MESSAGE_CONTROL3(GeofencingMsg_RegisterRegionComplete,
                     int /* thread_id */,
                     int /* request_id */,
                     content::GeofencingStatus /* status */)

// Reply in response to GeofencingHostMsg_UnregisterRegion
IPC_MESSAGE_CONTROL3(GeofencingMsg_UnregisterRegionComplete,
                     int /* thread_id */,
                     int /* request_id */,
                     content::GeofencingStatus /* status */)

// Reply in response to GeofencingHostMsg_GetRegisteredRegions
IPC_MESSAGE_CONTROL4(GeofencingMsg_GetRegisteredRegionsComplete,
                     int /* thread_id */,
                     int /* request_id */,
                     content::GeofencingStatus /* status */,
                     GeofencingRegistrations /* regions */)
