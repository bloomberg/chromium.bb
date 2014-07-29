// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for platform-native notifications using the Web Notification API.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START PlatformNotificationMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebNotificationPermission,
                          blink::WebNotificationPermissionLast)

// Messages sent from the browser to the renderer.

// Informs the renderer that the permission request for |request_id| is done,
// and has been settled with |result|.
IPC_MESSAGE_ROUTED2(PlatformNotificationMsg_PermissionRequestComplete,
                    int /* request_id */,
                    blink::WebNotificationPermission /* result */)

// Messages sent from the renderer to the browser.

// Requests permission to display platform notifications for |origin|.
IPC_MESSAGE_ROUTED2(PlatformNotificationHostMsg_RequestPermission,
                    GURL /* origin */,
                    int /* request_id */)
