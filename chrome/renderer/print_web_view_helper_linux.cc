// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/common/print_messages.h"
#include "content/common/view_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/point.h"

#if !defined(OS_CHROMEOS)
#include "base/process_util.h"
#endif  // !defined(OS_CHROMEOS)

using WebKit::WebFrame;
using WebKit::WebNode;

bool PrintWebViewHelper::CreatePreviewDocument(
    const PrintMsg_PrintPages_Params& params, WebKit::WebFrame* frame,
    WebKit::WebNode* node) {
  int page_count = 0;
  printing::PreviewMetafile metafile;
  if (!metafile.Init())
    return false;

  // Record the begin time.
  base::TimeTicks begin_time = base::TimeTicks::Now();

  if (!RenderPages(params, frame, node, false, &page_count, &metafile))
    return false;

  // Calculate the time taken to render the requested page for preview and add
  // the net time in the histogram.
  UMA_HISTOGRAM_TIMES("PrintPreview.RenderTime",
                      base::TimeTicks::Now() - begin_time);

  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

  PrintHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.data_size = buf_size;
  preview_params.document_cookie = params.params.document_cookie;
  preview_params.expected_pages_count = page_count;
  preview_params.modifiable = IsModifiable(frame, node);

  if (!CopyMetafileDataToSharedMem(&metafile,
                                   &(preview_params.metafile_data_handle))) {
    return false;
  }
  Send(new PrintHostMsg_PagesReadyForPreview(routing_id(), preview_params));
  return true;
}

bool PrintWebViewHelper::PrintPages(const PrintMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node) {
  int page_count = 0;
  bool send_expected_page_count =
#if defined(OS_CHROMEOS)
      false;
#else
      true;
#endif  // defined(OS_CHROMEOS)

  printing::NativeMetafile metafile;
  if (!metafile.Init())
    return false;

  if (!RenderPages(params, frame, node, send_expected_page_count, &page_count,
                   &metafile)) {
    return false;
  }

  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

#if defined(OS_CHROMEOS)
  int sequence_number = -1;
  base::FileDescriptor fd;

  // Ask the browser to open a file for us.
  Send(new PrintHostMsg_AllocateTempFileForPrinting(&fd, &sequence_number));
  if (!metafile.SaveToFD(fd))
    return false;

  // Tell the browser we've finished writing the file.
  Send(new PrintHostMsg_TempFileForPrintingWritten(sequence_number));
  return true;
#else
  PrintHostMsg_DidPrintPage_Params printed_page_params;
  printed_page_params.data_size = 0;
  printed_page_params.document_cookie = params.params.document_cookie;

  base::SharedMemoryHandle shared_mem_handle;
  Send(new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size,
                                                  &shared_mem_handle));
  if (!base::SharedMemory::IsHandleValid(shared_mem_handle)) {
    NOTREACHED() << "AllocateSharedMemoryBuffer returned bad handle";
    return false;
  }

  {
    base::SharedMemory shared_buf(shared_mem_handle, false);
    if (!shared_buf.Map(buf_size)) {
      NOTREACHED() << "Map failed";
      return false;
    }
    metafile.GetData(shared_buf.memory(), buf_size);
    printed_page_params.data_size = buf_size;
    shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                             &(printed_page_params.metafile_data_handle));
  }

  if (params.pages.empty()) {
    // Send the first page with a valid handle.
    printed_page_params.page_number = 0;
    Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));

    // Send the rest of the pages with an invalid metafile handle.
    printed_page_params.metafile_data_handle.fd = -1;
    for (int i = 1; i < page_count; ++i) {
      printed_page_params.page_number = i;
      Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  } else {
    // Send the first page with a valid handle.
    printed_page_params.page_number = params.pages[0];
    Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));

    // Send the rest of the pages with an invalid metafile handle.
    printed_page_params.metafile_data_handle.fd = -1;
    for (size_t i = 1; i < params.pages.size(); ++i) {
      printed_page_params.page_number = params.pages[i];
      Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  }
  return true;
#endif  // defined(OS_CHROMEOS)
}

bool PrintWebViewHelper::RenderPages(const PrintMsg_PrintPages_Params& params,
                                     WebKit::WebFrame* frame,
                                     WebKit::WebNode* node,
                                     bool send_expected_page_count,
                                     int* page_count,
                                     printing::Metafile* metafile) {
  PrintMsg_Print_Params printParams = params.params;
  scoped_ptr<skia::VectorCanvas> canvas;

  UpdatePrintableSizeInPrintParameters(frame, node, &printParams);

  {
    // Hack - when |prep_frame_view| goes out of scope, PrintEnd() gets called.
    // Doing this before closing |metafile| below ensures
    // webkit::ppapi::PluginInstance::PrintEnd() has a valid canvas/metafile to
    // save the final output to. See pepper_plugin_instance.cc for the whole
    // story.
    PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                                frame,
                                                node,
                                                frame->view());
    *page_count = prep_frame_view.GetExpectedPageCount();
    if (send_expected_page_count) {
      Send(new PrintHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                    printParams.document_cookie,
                                                    *page_count));
    }
    if (!*page_count)
      return false;

    PrintMsg_PrintPage_Params page_params;
    page_params.params = printParams;
    const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
    if (params.pages.empty()) {
      for (int i = 0; i < *page_count; ++i) {
        page_params.page_number = i;
        PrintPageInternal(page_params, canvas_size, frame, metafile, &canvas);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        PrintPageInternal(page_params, canvas_size, frame, metafile, &canvas);
      }
    }
  }

  metafile->FinishDocument();

  return true;
}

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame,
    printing::Metafile* metafile,
    scoped_ptr<skia::VectorCanvas>* canvas) {
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

  gfx::Size page_size(
      content_width_in_points + margin_right_in_points +
          margin_left_in_points,
      content_height_in_points + margin_top_in_points +
          margin_bottom_in_points);
  gfx::Rect content_area(margin_left_in_points, margin_top_in_points,
                         content_width_in_points, content_height_in_points);

  skia::PlatformDevice* device = metafile->StartPageForVectorCanvas(
      page_size, content_area, 1.0f);
  if (!device)
    return;

  canvas->reset(new skia::VectorCanvas(device));
  printing::MetafileSkiaWrapper::SetMetafileOnCanvas(canvas->get(), metafile);
  frame->printPage(params.page_number, canvas->get());

  // TODO(myhuang): We should handle transformation for paper margins.
  // TODO(myhuang): We should render the header and the footer.

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishPage())
    NOTREACHED() << "metafile failed";
}
