// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for desktop notification.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"

#define IPC_MESSAGE_START DesktopNotificationMsgStart

IPC_STRUCT_BEGIN(DesktopNotificationHostMsg_Show_Params)
  // URL which is the origin that created this notification.
  IPC_STRUCT_MEMBER(GURL, origin)

  // True if this is HTML
  IPC_STRUCT_MEMBER(bool, is_html)

  // URL which contains the HTML contents (if is_html is true), otherwise empty.
  IPC_STRUCT_MEMBER(GURL, contents_url)

  // Contents of the notification if is_html is false.
  IPC_STRUCT_MEMBER(GURL, icon_url)
  IPC_STRUCT_MEMBER(string16, title)
  IPC_STRUCT_MEMBER(string16, body)

  // Directionality of the notification.
  IPC_STRUCT_MEMBER(WebKit::WebTextDirection, direction)

  // ReplaceID if this notification should replace an existing one) may be
  // empty if no replacement is called for.
  IPC_STRUCT_MEMBER(string16, replace_id)

  // Notification ID for sending events back for this notification.
  IPC_STRUCT_MEMBER(int, notification_id)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Used to inform the renderer that the browser has displayed its
// requested notification.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PostDisplay,
                    int /* notification_id */)

// Used to inform the renderer that the browser has encountered an error
// trying to display a notification.
IPC_MESSAGE_ROUTED2(DesktopNotificationMsg_PostError,
                    int /* notification_id */,
                    string16 /* message */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED2(DesktopNotificationMsg_PostClose,
                    int /* notification_id */,
                    bool /* by_user */)

// Informs the renderer that one of its notifications was clicked on.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PostClick,
                    int /* notification_id */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PermissionRequestDone,
                    int /* request_id */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED1(DesktopNotificationHostMsg_Show,
                    DesktopNotificationHostMsg_Show_Params)

IPC_MESSAGE_ROUTED1(DesktopNotificationHostMsg_Cancel,
                    int /* notification_id */)

IPC_MESSAGE_ROUTED2(DesktopNotificationHostMsg_RequestPermission,
                    GURL /* origin */,
                    int /* callback_context */)

IPC_SYNC_MESSAGE_ROUTED1_1(DesktopNotificationHostMsg_CheckPermission,
                           GURL /* source page */,
                           int /* permission_result */)
