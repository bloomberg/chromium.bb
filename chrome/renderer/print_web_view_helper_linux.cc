// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/common/print_messages.h"
#include "content/public/renderer/render_thread.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/metafile_skia_wrapper.h"
#include "printing/page_size_margins.h"
#include "skia/ext/platform_device.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

#if !defined(OS_CHROMEOS)
#include "base/process_util.h"
#endif  // !defined(OS_CHROMEOS)

using WebKit::WebFrame;
using WebKit::WebNode;

bool PrintWebViewHelper::RenderPreviewPage(int page_number) {
  PrintMsg_PrintPage_Params page_params;
  page_params.params = print_preview_context_.print_params();
  page_params.page_number = page_number;
  scoped_ptr<printing::Metafile> draft_metafile;
  printing::Metafile* initial_render_metafile =
      print_preview_context_.metafile();
  if (print_preview_context_.IsModifiable() && is_print_ready_metafile_sent_) {
    draft_metafile.reset(new printing::PreviewMetafile);
    initial_render_metafile = draft_metafile.get();
  }

  base::TimeTicks begin_time = base::TimeTicks::Now();
  PrintPageInternal(page_params,
                    print_preview_context_.GetPrintCanvasSize(),
                    print_preview_context_.frame(), initial_render_metafile);
  print_preview_context_.RenderedPreviewPage(
      base::TimeTicks::Now() - begin_time);
  if (draft_metafile.get()) {
    draft_metafile->FinishDocument();
  } else if (print_preview_context_.IsModifiable() &&
             print_preview_context_.generate_draft_pages()){
    DCHECK(!draft_metafile.get());
    draft_metafile.reset(
        print_preview_context_.metafile()->GetMetafileForCurrentPage());
  }
  return PreviewPageRendered(page_number, draft_metafile.get());
}

bool PrintWebViewHelper::PrintPages(const PrintMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    const WebNode& node) {
  printing::NativeMetafile metafile;
  if (!metafile.Init())
    return false;

  PrepareFrameAndViewForPrint prep_frame_view(params.params, frame, node);
  int page_count = 0;
  if (!RenderPages(params, frame, node, &page_count, &prep_frame_view,
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

  base::SharedMemoryHandle shared_mem_handle =
      content::RenderThread::Get()->HostAllocateSharedMemoryBuffer(buf_size);
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
                                     const WebKit::WebNode& node,
                                     int* page_count,
                                     PrepareFrameAndViewForPrint* prepare,
                                     printing::Metafile* metafile) {
  PrintMsg_Print_Params print_params = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, prepare, &print_params);

  *page_count = prepare->GetExpectedPageCount();
  if (!*page_count)
    return false;

#if !defined(OS_CHROMEOS)
    Send(new PrintHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                  print_params.document_cookie,
                                                  *page_count));
#endif

  PrintMsg_PrintPage_Params page_params;
  page_params.params = print_params;
  const gfx::Size& canvas_size = prepare->GetPrintCanvasSize();
  if (params.pages.empty()) {
    for (int i = 0; i < *page_count; ++i) {
      page_params.page_number = i;
      PrintPageInternal(page_params, canvas_size, frame, metafile);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      page_params.page_number = params.pages[i];
      PrintPageInternal(page_params, canvas_size, frame, metafile);
    }
  }

  prepare->FinishPrinting();
  metafile->FinishDocument();
  return true;
}

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame,
    printing::Metafile* metafile) {
  printing::PageSizeMargins page_layout_in_points;
  GetPageSizeAndMarginsInPoints(frame, params.page_number, params.params,
                                &page_layout_in_points);

  gfx::Size page_size(
      page_layout_in_points.content_width +
          page_layout_in_points.margin_right +
          page_layout_in_points.margin_left,
      page_layout_in_points.content_height +
          page_layout_in_points.margin_top +
          page_layout_in_points.margin_bottom);
  gfx::Rect content_area(page_layout_in_points.margin_left,
                         page_layout_in_points.margin_top,
                         page_layout_in_points.content_width,
                         page_layout_in_points.content_height);

  SkDevice* device = metafile->StartPageForVectorCanvas(
      page_size, content_area, 1.0f);
  if (!device)
    return;

  // The printPage method take a reference to the canvas we pass down, so it
  // can't be a stack object.
  SkRefPtr<skia::VectorCanvas> canvas = new skia::VectorCanvas(device);
  canvas->unref();  // SkRefPtr and new both took a reference.
  printing::MetafileSkiaWrapper::SetMetafileOnCanvas(*canvas, metafile);
  skia::SetIsDraftMode(*canvas, is_print_ready_metafile_sent_);
  frame->printPage(params.page_number, canvas.get());

  if (params.params.display_header_footer) {
    // |page_number| is 0-based, so 1 is added.
    // The scale factor on Linux is 1.
    PrintHeaderAndFooter(canvas.get(), params.page_number + 1,
                         print_preview_context_.total_page_count(), 1,
                         page_layout_in_points, *header_footer_info_);
  }

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishPage())
    NOTREACHED() << "metafile failed";
}
