// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START AppShimMsgStart

// Signals that a previous LaunchApp message has been processed, and lets the
// app shim process know whether the app launch was successful.
IPC_MESSAGE_CONTROL1(AppShimMsg_LaunchApp_Done,
                     bool /* succeeded */)

// Tells the main Chrome process to launch a particular app with the given
// profile name and app id.
IPC_MESSAGE_CONTROL2(AppShimHostMsg_LaunchApp,
                     std::string /* profile name */,
                     std::string /* app id */)

// Sent when the user has indicated a desire to focus the app, either by
// clicking on the app's icon in the dock or by selecting it with Cmd+Tab. In
// response, Chrome brings the app's windows to the foreground.
IPC_MESSAGE_CONTROL0(AppShimHostMsg_FocusApp)
