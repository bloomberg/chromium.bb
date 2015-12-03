// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages sent from the host to the ARC instance.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>

#include "base/file_descriptor_posix.h"
#include "ipc/ipc_message_macros.h"

// Using relative paths since this file is shared between chromium and android.
#include "arc_message_types.h"
#include "arc_notification_types.h"

#define IPC_MESSAGE_START ArcInstanceMsgStart

// Registers a virtual input device on the container side.
// |name| is the device name, like "Chrome OS Keyboard".
// |device_type| is the device type, like "keyboard".
// The virtual device will be reading 'struct input_event's from |fd|.  The
// ownership of |fd| will be transferred to the receiver, so the sender must
// not close it.
IPC_MESSAGE_CONTROL3(ArcInstanceMsg_RegisterInputDevice,
                     std::string, /* name */
                     std::string, /* device_type */
                     base::FileDescriptor /* fd */)

// Sends a request to ARC to refresh a list of ARC apps.
// ArcInstanceMsg_RefreshApps is expected in response to this message. However,
// response may not be sent if ARC is not ready yet (boot completed event is
// not received).
IPC_MESSAGE_CONTROL0(ArcInstanceMsg_RefreshApps)

// Sends a request to ARC to launch an ARC app defined by |package| and
// |activity|, which cannot be empty.
IPC_MESSAGE_CONTROL2(ArcInstanceMsg_LaunchApp,
                     std::string, /* package */
                     std::string /* activity */)

// Sends a request to ARC for the ARC app icon of a required scale factor.
// Scale factor is an enum defined at ui/base/layout.h. App is defined by
// package and activity, which cannot be empty.
IPC_MESSAGE_CONTROL3(ArcInstanceMsg_RequestAppIcon,
                     std::string, /* package */
                     std::string, /* activity */
                     arc::ScaleFactor /* scale factor */)

// Sends an event from Chrome notification UI to Android.
// |event| is a type of occured event.
IPC_MESSAGE_CONTROL2(ArcInstanceMsg_SendNotificationEventToAndroid,
                     std::string /* key */,
                     arc::ArcNotificationEvent /* event */);
