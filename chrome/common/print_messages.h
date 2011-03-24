// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for printing.
// Multiply-included message file, hence no include guard.

#include "base/values.h"
#include "base/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START PrintMsgStart

// Parameters for a render request.
IPC_STRUCT_BEGIN(PrintMsg_Print_Params)
  // Physical size of the page, including non-printable margins,
  // in pixels according to dpi.
  IPC_STRUCT_MEMBER(gfx::Size, page_size)

  // In pixels according to dpi_x and dpi_y.
  IPC_STRUCT_MEMBER(gfx::Size, printable_size)

  // The y-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_MEMBER(int, margin_top)

  // The x-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_MEMBER(int, margin_left)

  // Specifies dots per inch.
  IPC_STRUCT_MEMBER(double, dpi)

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  IPC_STRUCT_MEMBER(double, min_shrink)

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  IPC_STRUCT_MEMBER(double, max_shrink)

  // Desired apparent dpi on paper.
  IPC_STRUCT_MEMBER(int, desired_dpi)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Should only print currently selected text.
  IPC_STRUCT_MEMBER(bool, selection_only)

  // Does the printer support alpha blending?
  IPC_STRUCT_MEMBER(bool, supports_alpha_blend)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PrintMsg_PrintPage_Params)
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  IPC_STRUCT_MEMBER(PrintMsg_Print_Params, params)

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in PrintMsg_Print_Params.
  IPC_STRUCT_MEMBER(int, page_number)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PrintMsg_PrintPages_Params)
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  IPC_STRUCT_MEMBER(PrintMsg_Print_Params, params)

  // If empty, this means a request to render all the printed pages.
  IPC_STRUCT_MEMBER(std::vector<int>, pages)
IPC_STRUCT_END()

// Parameters to describe a rendered document.
IPC_STRUCT_BEGIN(PrintHostMsg_DidPreviewDocument_Params)
  // A shared memory handle to metafile data.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, metafile_data_handle)

  // Size of metafile data.
  IPC_STRUCT_MEMBER(uint32, data_size)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Store the expected pages count.
  IPC_STRUCT_MEMBER(int, expected_pages_count)
IPC_STRUCT_END()

// Parameters to describe a rendered page.
IPC_STRUCT_BEGIN(PrintHostMsg_DidPrintPage_Params)
  // A shared memory handle to the EMF data. This data can be quite large so a
  // memory map needs to be used.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, metafile_data_handle)

  // Size of the metafile data.
  IPC_STRUCT_MEMBER(uint32, data_size)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Page number.
  IPC_STRUCT_MEMBER(int, page_number)

  // Shrink factor used to render this page.
  IPC_STRUCT_MEMBER(double, actual_shrink)

  // The size of the page the page author specified.
  IPC_STRUCT_MEMBER(gfx::Size, page_size)

  // The printable area the page author specified.
  IPC_STRUCT_MEMBER(gfx::Rect, content_area)

  // True if the page has visible overlays.
  IPC_STRUCT_MEMBER(bool, has_visible_overlays)
IPC_STRUCT_END()

// Parameters for the IPC message ViewHostMsg_ScriptedPrint
IPC_STRUCT_BEGIN(PrintHostMsg_ScriptedPrint_Params)
  IPC_STRUCT_MEMBER(int, routing_id)
  IPC_STRUCT_MEMBER(gfx::NativeViewId, host_window_id)
  IPC_STRUCT_MEMBER(int, cookie)
  IPC_STRUCT_MEMBER(int, expected_pages_count)
  IPC_STRUCT_MEMBER(bool, has_selection)
  IPC_STRUCT_MEMBER(bool, use_overlays)
IPC_STRUCT_END()


// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED0(PrintMsg_PrintNodeUnderContextMenu)

// Tells the renderer to print the print preview tab's PDF plugin without
// showing the print dialog.
IPC_MESSAGE_ROUTED1(PrintMsg_PrintForPrintPreview,
                    DictionaryValue /* settings*/)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages and switch back the CSS to display media type.
IPC_MESSAGE_ROUTED0(PrintMsg_PrintPages)

// Tells the render view that printing is done so it can clean up.
IPC_MESSAGE_ROUTED2(PrintMsg_PrintingDone,
                    int /* document_cookie */,
                    bool /* success */)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages for print preview using the given |settngs|.
IPC_MESSAGE_ROUTED1(PrintMsg_PrintPreview,
                    DictionaryValue /* settings */)

// Tells a renderer to stop blocking script initiated printing.
IPC_MESSAGE_ROUTED0(PrintMsg_ResetScriptedPrintCount)


// Messages sent from the renderer to the browser.

#if defined(OS_WIN)
// Duplicates a shared memory handle from the renderer to the browser. Then
// the renderer can flush the handle.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_DuplicateSection,
                           base::SharedMemoryHandle /* renderer handle */,
                           base::SharedMemoryHandle /* browser handle */)
#endif

// Tells the browser that the renderer is done calculating the number of
// rendered pages according to the specified settings.
IPC_MESSAGE_ROUTED2(PrintHostMsg_DidGetPrintedPagesCount,
                    int /* rendered document cookie */,
                    int /* number of rendered pages */)

// Sends back to the browser the rendered "printed page" that was requested by
// a ViewMsg_PrintPage message or from scripted printing. The memory handle in
// this message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidPrintPage,
                    PrintHostMsg_DidPrintPage_Params /* page content */)

// The renderer wants to know the default print settings.
IPC_SYNC_MESSAGE_ROUTED0_1(PrintHostMsg_GetDefaultPrintSettings,
                           PrintMsg_Print_Params /* default_settings */)

// The renderer wants to update the current print settings with new
// |job_settings|.
IPC_SYNC_MESSAGE_ROUTED2_1(PrintHostMsg_UpdatePrintSettings,
                           int /* document_cookie */,
                           DictionaryValue /* job_settings */,
                           PrintMsg_PrintPages_Params /* current_settings */)

// It's the renderer that controls the printing process when it is generated
// by javascript. This step is about showing UI to the user to select the
// final print settings. The output parameter is the same as
// ViewMsg_PrintPages which is executed implicitly.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_ScriptedPrint,
                           PrintHostMsg_ScriptedPrint_Params,
                           PrintMsg_PrintPages_Params
                               /* settings chosen by the user*/)

#if defined(USE_X11)
// Asks the browser to create a temporary file for the renderer to fill
// in resulting NativeMetafile in printing.
IPC_SYNC_MESSAGE_CONTROL0_2(PrintHostMsg_AllocateTempFileForPrinting,
                            base::FileDescriptor /* temp file fd */,
                            int /* fd in browser*/)
IPC_MESSAGE_CONTROL1(PrintHostMsg_TempFileForPrintingWritten,
                     int /* fd in browser */)
#endif

// Asks the browser to do print preview for the node under the context menu.
IPC_MESSAGE_ROUTED0(PrintHostMsg_PrintPreviewNodeUnderContextMenu)

// Asks the browser to do print preview for window.print().
IPC_MESSAGE_ROUTED0(PrintHostMsg_ScriptInitiatedPrintPreview)

// Sends back to the browser the rendered "printed document" for preview that
// was requested by a PrintMsg_PrintPreview message. The memory handle in this
// message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(PrintHostMsg_PagesReadyForPreview,
                    PrintHostMsg_DidPreviewDocument_Params /* params */)
