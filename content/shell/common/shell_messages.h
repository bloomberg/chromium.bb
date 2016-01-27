// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "components/test_runner/layout_dump_flags.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_state.h"
#include "content/shell/common/leak_detection_result.h"
#include "content/shell/common/shell_test_configuration.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START ShellMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::ShellTestConfiguration)
IPC_STRUCT_TRAITS_MEMBER(current_working_directory)
IPC_STRUCT_TRAITS_MEMBER(temp_path)
IPC_STRUCT_TRAITS_MEMBER(test_url)
IPC_STRUCT_TRAITS_MEMBER(enable_pixel_dumping)
IPC_STRUCT_TRAITS_MEMBER(allow_external_pages)
IPC_STRUCT_TRAITS_MEMBER(expected_pixel_hash)
IPC_STRUCT_TRAITS_MEMBER(initial_size)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    test_runner::LayoutDumpMode,
    test_runner::LayoutDumpMode::DUMP_AS_TEXT,
    test_runner::LayoutDumpMode::DUMP_SCROLL_POSITIONS)

IPC_STRUCT_TRAITS_BEGIN(test_runner::LayoutDumpFlags)
  IPC_STRUCT_TRAITS_MEMBER(main_dump_mode)
  IPC_STRUCT_TRAITS_MEMBER(dump_as_printed)
  IPC_STRUCT_TRAITS_MEMBER(dump_child_frames)
  IPC_STRUCT_TRAITS_MEMBER(dump_line_box_trees)
  IPC_STRUCT_TRAITS_MEMBER(debug_render_tree)
IPC_STRUCT_TRAITS_END()

// Tells the renderer to reset all test runners.
IPC_MESSAGE_ROUTED0(ShellViewMsg_Reset)

// Sets the path to the WebKit checkout.
IPC_MESSAGE_CONTROL1(ShellViewMsg_SetWebKitSourceDir,
                     base::FilePath /* webkit source dir */)

// Sets the initial configuration to use for layout tests.
IPC_MESSAGE_ROUTED1(ShellViewMsg_SetTestConfiguration,
                    content::ShellTestConfiguration)

// Tells the main window that a secondary window in a different process invoked
// notifyDone().
IPC_MESSAGE_ROUTED0(ShellViewMsg_NotifyDone)

// Pushes a snapshot of the current session history from the browser process.
// This includes only information about those RenderViews that are in the
// same process as the main window of the layout test and that are the current
// active RenderView of their WebContents.
IPC_MESSAGE_ROUTED3(
    ShellViewMsg_SessionHistory,
    std::vector<int> /* routing_ids */,
    std::vector<std::vector<content::PageState> > /* session_histories */,
    std::vector<unsigned> /* current_entry_indexes */)

IPC_MESSAGE_ROUTED0(ShellViewMsg_TryLeakDetection)

// Asks a frame to dump its contents into a string and send them back over IPC.
IPC_MESSAGE_ROUTED1(ShellViewMsg_LayoutDumpRequest,
                    test_runner::LayoutDumpFlags)

// Notifies BlinkTestRunner that the layout dump has completed
// (and that it can proceed with finishing up the test).
IPC_MESSAGE_ROUTED1(ShellViewMsg_LayoutDumpCompleted,
                    std::string /* completed/stitched layout dump */)

// Send a text dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TextDump,
                    std::string /* dump */)

// Asks the browser process to perform a layout dump (potentially spanning
// multiple cross-process frames) using the given flags.  This triggers
// multiple ShellViewMsg_LayoutDumpRequest / ShellViewHostMsg_LayoutDumpResponse
// messages and ends with sending of ShellViewMsg_LayoutDumpCompleted.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_InitiateLayoutDump,
                    test_runner::LayoutDumpFlags)

// Sends a layout dump of a frame (response to ShellViewMsg_LayoutDumpRequest).
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_LayoutDumpResponse, std::string /* dump */)

// Send an image dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ImageDump,
                    std::string /* actual pixel hash */,
                    SkBitmap /* image */)

// Send an audio dump to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_AudioDump,
                    std::vector<unsigned char> /* audio data */)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_TestFinished)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ResetDone)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_TestFinishedInSecondaryWindow)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_OverridePreferences,
                    content::WebPreferences /* preferences */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_PrintMessage,
                    std::string /* message */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ClearDevToolsLocalStorage)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ShowDevTools,
                    std::string /* settings */,
                    std::string /* frontend_url */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseDevTools)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_GoToOffset,
                    int /* offset */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_Reload)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_LoadURLForFrame,
                    GURL /* url */,
                    std::string /* frame_name */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CaptureSessionHistory)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseRemainingWindows)

IPC_STRUCT_TRAITS_BEGIN(content::LeakDetectionResult)
IPC_STRUCT_TRAITS_MEMBER(leaked)
IPC_STRUCT_TRAITS_MEMBER(detail)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_LeakDetectionDone,
                    content::LeakDetectionResult /* result */)

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetBluetoothManualChooser,
                    bool /* enable */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_GetBluetoothManualChooserEvents)
IPC_MESSAGE_ROUTED1(ShellViewMsg_ReplyBluetoothManualChooserEvents,
                    std::vector<std::string> /* events */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_SendBluetoothManualChooserEvent,
                    std::string /* event */,
                    std::string /* argument */)
