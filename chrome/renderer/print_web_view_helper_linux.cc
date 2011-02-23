// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "printing/native_metafile.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

#if !defined(OS_CHROMEOS)
#include "base/process_util.h"
#endif  // !defined(OS_CHROMEOS)

using WebKit::WebFrame;
using WebKit::WebNode;

void PrintWebViewHelper::CreatePreviewDocument(
    const ViewMsg_PrintPages_Params& params, WebFrame* frame) {
  // We only can use PDF in the renderer because Cairo needs to create a
  // temporary file for a PostScript surface.
  printing::NativeMetafile metafile(printing::NativeMetafile::PDF);
  int page_count = 0;

  if (!RenderPages(params, frame, false, &page_count, &metafile))
    return;

  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

  ViewHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.document_cookie = params.params.document_cookie;
  preview_params.expected_pages_count = page_count;
  preview_params.data_size = buf_size;

  if (!CopyMetafileDataToSharedMem(&metafile,
                                   &(preview_params.metafile_data_handle))) {
    preview_params.expected_pages_count = 0;
    preview_params.data_size = 0;
  }
  Send(new ViewHostMsg_PagesReadyForPreview(routing_id(), preview_params));
}

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node) {
  // We only can use PDF in the renderer because Cairo needs to create a
  // temporary file for a PostScript surface.
  printing::NativeMetafile metafile(printing::NativeMetafile::PDF);
  int page_count = 0;
  bool send_expected_page_count =
#if defined(OS_CHROMEOS)
      false;
#else
      true;
#endif  // defined(OS_CHROMEOS)

  if (!RenderPages(params, frame, send_expected_page_count, &page_count,
                   &metafile)) {
    return;
  }

  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

#if defined(OS_CHROMEOS)
  int sequence_number = -1;
  base::FileDescriptor fd;

  // Ask the browser to open a file for us.
  if (!Send(new ViewHostMsg_AllocateTempFileForPrinting(&fd,
                                                        &sequence_number))) {
    return;
  }
  if (!metafile.SaveTo(fd))
    return;

  // Tell the browser we've finished writing the file.
  Send(new ViewHostMsg_TempFileForPrintingWritten(sequence_number));
#else
  ViewHostMsg_DidPrintPage_Params printed_page_params;
  printed_page_params.data_size = 0;
  printed_page_params.document_cookie = params.params.document_cookie;

  base::SharedMemoryHandle shared_mem_handle;
  if (!Send(new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size,
                                                       &shared_mem_handle))) {
    NOTREACHED() << "AllocateSharedMemoryBuffer failed";
    return;
  }
  if (!base::SharedMemory::IsHandleValid(shared_mem_handle)) {
    NOTREACHED() << "AllocateSharedMemoryBuffer returned bad handle";
    return;
  }

  {
    base::SharedMemory shared_buf(shared_mem_handle, false);
    if (!shared_buf.Map(buf_size)) {
      NOTREACHED() << "Map failed";
      return;
    }
    metafile.GetData(shared_buf.memory(), buf_size);
    printed_page_params.data_size = buf_size;
    shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                             &(printed_page_params.metafile_data_handle));
  }

  // Send the first page with a valid handle.
  printed_page_params.page_number = 0;
  Send(new ViewHostMsg_DidPrintPage(routing_id(), printed_page_params));

  // Send the rest of the pages with an invalid metafile handle.
  printed_page_params.metafile_data_handle.fd = -1;
  if (params.pages.empty()) {
    for (int i = 1; i < page_count; ++i) {
      printed_page_params.page_number = i;
      Send(new ViewHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  } else {
    for (size_t i = 1; i < params.pages.size(); ++i) {
      printed_page_params.page_number = params.pages[i];
      Send(new ViewHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  }
#endif  // defined(OS_CHROMEOS)
}

bool PrintWebViewHelper::RenderPages(const ViewMsg_PrintPages_Params& params,
                                     WebFrame* frame,
                                     bool send_expected_page_count,
                                     int* page_count,
                                     printing::NativeMetafile* metafile) {
  ViewMsg_Print_Params printParams = params.params;
  skia::VectorCanvas* canvas = NULL;

  {
    // Hack - when |prep_frame_view| goes out of scope, PrintEnd() gets called.
    // Doing this before closing |metafile| below ensures
    // webkit::ppapi::PluginInstance::PrintEnd() has a valid canvas/metafile to
    // save the final output to. See pepper_plugin_instance.cc for the whole
    // story.
    PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                                frame,
                                                NULL,
                                                frame->view());
    *page_count = prep_frame_view.GetExpectedPageCount();
    if (send_expected_page_count) {
      Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                   printParams.document_cookie,
                                                   *page_count));
    }
    if (!*page_count)
      return false;

    metafile->Init();

    ViewMsg_PrintPage_Params page_params;
    page_params.params = printParams;
    const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
    if (params.pages.empty()) {
      for (int i = 0; i < *page_count; ++i) {
        page_params.page_number = i;
        delete canvas;
        PrintPage(page_params, canvas_size, frame, metafile, &canvas);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        delete canvas;
        PrintPage(page_params, canvas_size, frame, metafile, &canvas);
      }
    }
  }

  delete canvas;
  metafile->Close();

  return true;
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame,
                                   printing::NativeMetafile* metafile,
                                   skia::VectorCanvas** canvas) {
  double content_width_in_points;
  double content_height_in_points;
  double margin_top_in_points;
  double margin_right_in_points;
  double margin_bottom_in_points;
  double margin_left_in_points;
  GetPageSizeAndMarginsInPoints(frame,
                                params.page_number,
                                params.params,
                                &content_width_in_points,
                                &content_height_in_points,
                                &margin_top_in_points,
                                &margin_right_in_points,
                                &margin_bottom_in_points,
                                &margin_left_in_points);

  cairo_t* cairo_context =
      metafile->StartPage(content_width_in_points,
                          content_height_in_points,
                          margin_top_in_points,
                          margin_right_in_points,
                          margin_bottom_in_points,
                          margin_left_in_points);
  if (!cairo_context)
    return;

  *canvas = new skia::VectorCanvas(cairo_context,
                                   canvas_size.width(),
                                   canvas_size.height());
  frame->printPage(params.page_number, *canvas);

  // TODO(myhuang): We should handle transformation for paper margins.
  // TODO(myhuang): We should render the header and the footer.

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishPage())
    NOTREACHED() << "metafile failed";
}
