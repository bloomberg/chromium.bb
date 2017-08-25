// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/histogram.h"
#include "components/printing/common/print_messages.h"
#include "printing/features/features.h"
#include "printing/metafile_skia_wrapper.h"
#include "printing/page_size_margins.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace printing {

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
bool PrintRenderFrameHelper::PrintPagesNative(blink::WebLocalFrame* frame,
                                              int page_count) {
  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  const PrintMsg_Print_Params& print_params = params.params;

  std::vector<int> printed_pages = GetPrintedPages(params, page_count);
  if (printed_pages.empty())
    return false;

  if (delegate_->UseSingleMetafile()) {
    PrintPagesInternal(print_params, printed_pages, page_count, frame);
    return true;
  }

  for (int page_number : printed_pages)
    PrintPagesInternal(print_params, std::vector<int>{page_number}, page_count,
                       frame);
  return true;
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

void PrintRenderFrameHelper::PrintPagesInternal(
    const PrintMsg_Print_Params& params,
    const std::vector<int>& printed_pages,
    int page_count,
    blink::WebLocalFrame* frame) {
  PdfMetafileSkia metafile(params.printed_doc_type);
  CHECK(metafile.Init());

  gfx::Size page_size_in_dpi;
  gfx::Rect content_area_in_dpi;
  for (int page_number : printed_pages) {
    RenderPage(params, page_number, page_count, frame, false, &metafile,
               &page_size_in_dpi, &content_area_in_dpi);
  }
  metafile.FinishDocument();

  PrintHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = metafile.GetDataSize();
  page_params.document_cookie = params.document_cookie;
  page_params.page_size = page_size_in_dpi;
  page_params.content_area = content_area_in_dpi;

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(metafile,
                                   &page_params.metafile_data_handle)) {
    // TODO(thestig): Fail and return false instead.
    page_params.data_size = 0;
  }

  for (int page_number : printed_pages) {
    page_params.page_number = page_number;
    Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
    page_params.metafile_data_handle = base::SharedMemoryHandle();
  }
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
bool PrintRenderFrameHelper::RenderPreviewPage(
    int page_number,
    const PrintMsg_Print_Params& print_params) {
  std::unique_ptr<PdfMetafileSkia> draft_metafile;
  PdfMetafileSkia* initial_render_metafile = print_preview_context_.metafile();

  bool render_to_draft =
      print_preview_context_.IsModifiable() && is_print_ready_metafile_sent_;

  if (render_to_draft) {
    draft_metafile =
        base::MakeUnique<PdfMetafileSkia>(print_params.printed_doc_type);
    CHECK(draft_metafile->Init());
    initial_render_metafile = draft_metafile.get();
  }

  base::TimeTicks begin_time = base::TimeTicks::Now();
  gfx::Size page_size;
  RenderPage(print_params, page_number,
             print_preview_context_.total_page_count(),
             print_preview_context_.prepared_frame(), true,
             initial_render_metafile, &page_size, NULL);
  print_preview_context_.RenderedPreviewPage(base::TimeTicks::Now() -
                                             begin_time);

  if (draft_metafile.get()) {
    draft_metafile->FinishDocument();
  } else {
    if (print_preview_context_.IsModifiable() &&
        print_preview_context_.generate_draft_pages()) {
      DCHECK(!draft_metafile.get());
      draft_metafile =
          print_preview_context_.metafile()->GetMetafileForCurrentPage(
              print_params.printed_doc_type);
    }
  }
  return PreviewPageRendered(page_number, draft_metafile.get());
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintRenderFrameHelper::RenderPage(const PrintMsg_Print_Params& params,
                                        int page_number,
                                        int page_count,
                                        blink::WebLocalFrame* frame,
                                        bool is_preview,
                                        PdfMetafileSkia* metafile,
                                        gfx::Size* page_size,
                                        gfx::Rect* content_rect) {
  double scale_factor =
      params.scale_factor >= kEpsilon ? params.scale_factor : 1.0f;
  double webkit_shrink_factor = frame->GetPrintPageShrink(page_number);
  PageSizeMargins page_layout_in_points;
  gfx::Rect content_area;

  ComputePageLayoutInPointsForCss(frame, page_number, params,
                                  ignore_css_margins_, &scale_factor,
                                  &page_layout_in_points);
  GetPageSizeAndContentAreaFromPageLayout(page_layout_in_points, page_size,
                                          &content_area);
  if (content_rect)
    *content_rect = content_area;

  scale_factor *= webkit_shrink_factor;

  gfx::Rect canvas_area =
      params.display_header_footer ? gfx::Rect(*page_size) : content_area;

  {
    cc::PaintCanvas* canvas = metafile->GetVectorCanvasForNewPage(
        *page_size, canvas_area, scale_factor);
    if (!canvas)
      return;

    MetafileSkiaWrapper::SetMetafileOnCanvas(canvas, metafile);
    cc::SetIsPreviewMetafile(canvas, is_preview);
    if (params.display_header_footer) {
      PrintHeaderAndFooter(static_cast<blink::WebCanvas*>(canvas),
                           page_number + 1, page_count, *frame, scale_factor,
                           page_layout_in_points, params);
    }
    RenderPageContent(frame, page_number, canvas_area, content_area,
                      scale_factor, static_cast<blink::WebCanvas*>(canvas));
  }

  // Done printing. Close the device context to retrieve the compiled metafile.
  metafile->FinishPage();
}

}  // namespace printing
