// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define IPC_MESSAGE_START ShellMsgStart

IPC_STRUCT_BEGIN(ShellViewMsg_SetTestConfiguration_Params)
  // The current working directory.
  IPC_STRUCT_MEMBER(base::FilePath, current_working_directory)

  // The temporary directory of the system.
  IPC_STRUCT_MEMBER(base::FilePath, temp_path)

  // The URL of the current layout test.
  IPC_STRUCT_MEMBER(GURL, test_url)

  // True if pixel tests are enabled.
  IPC_STRUCT_MEMBER(bool, enable_pixel_dumping)

  // The layout test timeout in milliseconds.
  IPC_STRUCT_MEMBER(int, layout_test_timeout)

  // True if tests can open external URLs
  IPC_STRUCT_MEMBER(bool, allow_external_pages)

  // The expected MD5 hash of the pixel results.
  IPC_STRUCT_MEMBER(std::string, expected_pixel_hash)
IPC_STRUCT_END()

// Tells the renderer to reset all test runners.
IPC_MESSAGE_CONTROL0(ShellViewMsg_ResetAll)

// Sets the path to the WebKit checkout.
IPC_MESSAGE_CONTROL1(ShellViewMsg_SetWebKitSourceDir,
                     base::FilePath /* webkit source dir */)

// Loads the hyphen dictionary used for layout tests.
IPC_MESSAGE_CONTROL1(ShellViewMsg_LoadHyphenDictionary,
                     IPC::PlatformFileForTransit /* dict_file */)

// Sets the initial configuration to use for layout tests.
IPC_MESSAGE_ROUTED1(ShellViewMsg_SetTestConfiguration,
                    ShellViewMsg_SetTestConfiguration_Params)

// Pushes a snapshot of the current session history from the browser process.
// This includes only information about those RenderViews that are in the
// same process as the main window of the layout test and that are the current
// active RenderView of their WebContents.
IPC_MESSAGE_ROUTED3(
    ShellViewMsg_SessionHistory,
    std::vector<int> /* routing_ids */,
    std::vector<std::vector<std::string> > /* session_histories */,
    std::vector<unsigned> /* current_entry_indexes */)

// Send a text dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TextDump,
                    std::string /* dump */)

// Send an image dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ImageDump,
                    std::string /* actual pixel hash */,
                    SkBitmap /* image */)

// Send an audio dump to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_AudioDump,
                    std::vector<unsigned char> /* audio data */)

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
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_GoToOffset,
                    int /* offset */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_Reload)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_LoadURLForFrame,
                    GURL /* url */,
                    std::string /* frame_name */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ClearAllDatabases)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetDatabaseQuota,
                    int /* quota */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_AcceptAllCookies,
                    bool /* accept */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetClientWindowRect,
                    gfx::Rect /* rect */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetDeviceScaleFactor,
                    float /* factor */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetFocus,
                    bool /* focus */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CaptureSessionHistory)
