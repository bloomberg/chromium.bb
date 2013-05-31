// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "apps/app_shim/app_shim_launch.h"
#include "base/files/file_path.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"

#define IPC_MESSAGE_START AppShimMsgStart

IPC_ENUM_TRAITS(apps::AppShimLaunchType)

// Signals that a previous LaunchApp message has been processed, and lets the
// app shim process know whether it was registered successfully.
IPC_MESSAGE_CONTROL1(AppShimMsg_LaunchApp_Done,
                     bool /* succeeded */)

// Signals to the main Chrome process that a shim has started indicating the
// profile and app_id that the shim should be associated with and whether to
// launch the app immediately.
IPC_MESSAGE_CONTROL3(AppShimHostMsg_LaunchApp,
                     base::FilePath /* profile dir */,
                     std::string /* app id */,
                     apps::AppShimLaunchType /* launch type */)

// Sent when the user has indicated a desire to focus the app, either by
// clicking on the app's icon in the dock or by selecting it with Cmd+Tab. In
// response, Chrome brings the app's windows to the foreground.
IPC_MESSAGE_CONTROL0(AppShimHostMsg_FocusApp)

// Sent when the shim process receives a request to terminate. Once all of the
// app's windows have closed, and the extension is unloaded, the AppShimHost
// closes the channel. The shim process then completes the terminate request
// and exits.
IPC_MESSAGE_CONTROL0(AppShimHostMsg_QuitApp)
