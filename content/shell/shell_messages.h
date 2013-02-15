// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define IPC_MESSAGE_START ShellMsgStart

// Tells the renderer to reset all test runners.
IPC_MESSAGE_CONTROL0(ShellViewMsg_ResetAll)

// Sets the path to the WebKit checkout.
IPC_MESSAGE_CONTROL1(ShellViewMsg_SetWebKitSourceDir,
                     base::FilePath /* webkit source dir */)

// Sets the initial configuration to use for layout tests.
IPC_MESSAGE_ROUTED5(ShellViewMsg_SetTestConfiguration,
                    base::FilePath /* current_working_directory */,
                    bool /* enable_pixel_dumping */,
                    int /* layout_test_timeout */,
                    bool /* allow_external_pages */,
                    std::string /* expected pixel hash */)

// Send a text dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TextDump,
                    std::string /* dump */)

// Send an image dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ImageDump,
                    std::string /* actual pixel hash */,
                    SkBitmap /* image */)

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TestFinished,
                    bool /* did_timeout */)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_OverridePreferences,
                    webkit_glue::WebPreferences /* preferences */)
IPC_SYNC_MESSAGE_ROUTED1_1(ShellViewHostMsg_RegisterIsolatedFileSystem,
                           std::vector<base::FilePath> /* absolute_filenames */,
                           std::string /* filesystem_id */)
IPC_SYNC_MESSAGE_ROUTED1_1(ShellViewHostMsg_ReadFileToString,
                           base::FilePath /* local path */,
                           std::string /* contents */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_PrintMessage,
                    std::string /* message */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ShowDevTools)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseDevTools)

IPC_MESSAGE_ROUTED2(ShellViewHostMsg_NotImplemented,
                    std::string /* object_name */,
                    std::string /* property_name */)
