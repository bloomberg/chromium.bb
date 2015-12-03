// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_macros.h"

// Using relative paths since this file is shared between chromium and android.
#include "arc_message_types.h"
#include "arc_notification_types.h"

IPC_ENUM_TRAITS_MIN_MAX_VALUE(arc::ScaleFactor,
                              arc::ScaleFactor::SCALE_FACTOR_100P,
                              arc::ScaleFactor::NUM_SCALE_FACTORS);

IPC_STRUCT_TRAITS_BEGIN(arc::AppInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(package)
  IPC_STRUCT_TRAITS_MEMBER(activity)
IPC_STRUCT_TRAITS_END()

// Enum for notification type.
IPC_ENUM_TRAITS_MAX_VALUE(arc::ArcNotificationType,
                          arc::ArcNotificationType::LAST)

// Struct for notification data.
IPC_STRUCT_TRAITS_BEGIN(arc::ArcNotificationData)
  IPC_STRUCT_TRAITS_MEMBER(key)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(message)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(icon_mimetype)
  IPC_STRUCT_TRAITS_MEMBER(icon_data)
  IPC_STRUCT_TRAITS_MEMBER(priority)
  IPC_STRUCT_TRAITS_MEMBER(time)
  IPC_STRUCT_TRAITS_MEMBER(progress_current)
  IPC_STRUCT_TRAITS_MEMBER(progress_max)
IPC_STRUCT_TRAITS_END()

// Enum for notification event types.
IPC_ENUM_TRAITS_MAX_VALUE(arc::ArcNotificationEvent,
                          arc::ArcNotificationEvent::LAST)
