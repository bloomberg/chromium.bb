// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for platform-native notifications using the Web Notification API.
// Multiply-included message file, hence no include guard.

#include "content/public/common/platform_notification_data.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationPermission.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define IPC_MESSAGE_START PlatformNotificationMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebNotificationPermission,
                          blink::WebNotificationPermissionLast)

IPC_ENUM_TRAITS_MAX_VALUE(
    content::PlatformNotificationData::NotificationDirection,
    content::PlatformNotificationData::NotificationDirectionLast)

IPC_STRUCT_TRAITS_BEGIN(content::PlatformNotificationData)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(direction)
  IPC_STRUCT_TRAITS_MEMBER(lang)
  IPC_STRUCT_TRAITS_MEMBER(body)
  IPC_STRUCT_TRAITS_MEMBER(tag)
  IPC_STRUCT_TRAITS_MEMBER(icon)
  IPC_STRUCT_TRAITS_MEMBER(silent)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

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

IPC_MESSAGE_CONTROL4(PlatformNotificationHostMsg_Show,
                     int /* notification_id */,
                     GURL /* origin */,
                     SkBitmap /* icon */,
                     content::PlatformNotificationData /* notification_data */)

IPC_MESSAGE_CONTROL4(PlatformNotificationHostMsg_ShowPersistent,
                     int64 /* service_worker_provider_id */,
                     GURL /* origin */,
                     SkBitmap /* icon */,
                     content::PlatformNotificationData /* notification_data */)

IPC_MESSAGE_CONTROL1(PlatformNotificationHostMsg_Close,
                     int /* notification_id */)

IPC_MESSAGE_CONTROL2(PlatformNotificationHostMsg_ClosePersistent,
                     GURL /* origin */,
                     std::string /* persistent_notification_id */)

IPC_SYNC_MESSAGE_CONTROL1_1(PlatformNotificationHostMsg_CheckPermission,
                            GURL /* origin */,
                            blink::WebNotificationPermission /* result */)
