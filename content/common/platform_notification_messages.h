// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for platform-native notifications using the Web Notification API.
// Multiply-included message file, hence no include guard.

#include "content/public/common/show_desktop_notification_params.h"
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

// Informs the renderer that the browser has displayed the notification.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidShow,
                     int /* notification_id */)

// Informs the renderer that the notification has been closed.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidClose,
                     int /* notification_id */)

// Informs the renderer that the notification has been clicked on.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidClick,
                     int /* notification_id */)

// Messages sent from the renderer to the browser.

// Requests permission to display platform notifications for |origin|.
IPC_MESSAGE_ROUTED2(PlatformNotificationHostMsg_RequestPermission,
                    GURL /* origin */,
                    int /* request_id */)

IPC_MESSAGE_CONTROL2(PlatformNotificationHostMsg_Show,
                     int /* notification_id */,
                     content::ShowDesktopNotificationHostMsgParams /* params */)

IPC_MESSAGE_CONTROL1(PlatformNotificationHostMsg_Close,
                     int /* notification_id */)

IPC_SYNC_MESSAGE_CONTROL1_1(PlatformNotificationHostMsg_CheckPermission,
                            GURL /* origin */,
                            blink::WebNotificationPermission /* result */)
