// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"

#include "ipc/ipc_message_macros.h"
#include "ui/gfx/native_widget_types.h"

#define IPC_MESSAGE_START ChromePluginMsgStart

//-----------------------------------------------------------------------------
// ChromePluginHost messages
// These are messages sent from the plugin process to the browser process.

// Used to retrieve the plugin finder URL.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromePluginProcessHostMsg_GetPluginFinderUrl,
                            std::string /* plugin finder URL */)

// Used to display an infobar containing missing plugin information in the
// chrome browser process.
IPC_MESSAGE_CONTROL4(ChromePluginProcessHostMsg_MissingPluginStatus,
                     int /* status */,
                     int /* render_process_id */,
                     int /* render_view_id */,
                     gfx::NativeWindow /* The plugin window handle */)

#if defined(OS_WIN)
// Used to initiate a download on the plugin installer file from the URL
// passed in.
IPC_MESSAGE_CONTROL3(ChromePluginProcessHostMsg_DownloadUrl,
                     std::string /* URL */,
                     gfx::NativeWindow /* caller window */,
                     int /* render_process_id */)

#endif  // OS_WIN
