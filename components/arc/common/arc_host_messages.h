// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages sent from the ARC instance to the host.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "ipc/ipc_message_macros.h"

// Using relative paths since this file is shared between chromium and android.
#include "arc_message_types.h"
#include "arc_notification_types.h"

#define IPC_MESSAGE_START ArcInstanceHostMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(arc::InstanceBootPhase, arc::InstanceBootPhase::LAST)

IPC_MESSAGE_CONTROL1(ArcInstanceHostMsg_InstanceBootPhase,
                     arc::InstanceBootPhase)

// Sends a list of available ARC apps to Chrome. Members of AppInfo must contain
// non-empty string. This message is sent in response to
// ArcInstanceMsg_RefreshApps message from Chrome to ARC and when ARC receives
// boot completed notification.
IPC_MESSAGE_CONTROL1(ArcInstanceHostMsg_AppListRefreshed,
                     std::vector<arc::AppInfo> /* apps */)

// Sends an icon of required |scale_factor| for specific ARC app. The app is
// defined by |package| and |activity|. The icon content cannot be empty and
// must match to |scale_factor| assuming 48x48 for SCALE_FACTOR_100P.
// |scale_factor| is an enum defined at ui/base/layout.h. This message is sent
// in response to ArcInstanceMsg_RequestIcon from Chrome to ARC.
IPC_MESSAGE_CONTROL4(ArcInstanceHostMsg_AppIcon,
                     std::string, /* package */
                     std::string, /* activity */
                     arc::ScaleFactor, /* scale_factor */
                     std::vector<uint8_t> /* icon_png_data */)

// Tells the Chrome that a notification is posted (created or updated) on
// Android.
// |notification_data| is the data of notification (id, texts, icon and ...).
IPC_MESSAGE_CONTROL1(ArcInstanceHostMsg_NotificationPosted,
                     arc::ArcNotificationData /* notification_data */);

// Notifies that a notification is removed on Android.
// |key| is the identifier of the notification.
IPC_MESSAGE_CONTROL1(ArcInstanceHostMsg_NotificationRemoved,
                     std::string /* key */);
