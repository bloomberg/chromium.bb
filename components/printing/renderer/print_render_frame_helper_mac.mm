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
  PrintPageInternal(print_params, page_number,
                    print_preview_context_.total_page_count(),
                    print_preview_context_.prepared_frame(),
                    initial_render_metafile, nullptr, nullptr);
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

void PrintRenderFrameHelper::PrintPageInternal(
    const PrintMsg_Print_Params& params,
    int page_number,
    int page_count,
    blink::WebLocalFrame* frame,
    PdfMetafileSkia* metafile,
    gfx::Size* page_size_in_dpi,
    gfx::Rect* content_rect_in_dpi) {
  double css_scale_factor =
      params.scale_factor >= kEpsilon ? params.scale_factor : 1.0f;

  PageSizeMargins page_layout_in_points;
  ComputePageLayoutInPointsForCss(frame, page_number, params,
                                  ignore_css_margins_, &css_scale_factor,
                                  &page_layout_in_points);

  gfx::Size page_size;
  gfx::Rect content_area;
  GetPageSizeAndContentAreaFromPageLayout(page_layout_in_points, &page_size,
                                          &content_area);

  if (page_size_in_dpi)
    *page_size_in_dpi = page_size;

  if (content_rect_in_dpi)
    *content_rect_in_dpi = content_area;

  gfx::Rect canvas_area =
      params.display_header_footer ? gfx::Rect(page_size) : content_area;

  double webkit_page_shrink_factor = frame->GetPrintPageShrink(page_number);
  float scale_factor = css_scale_factor * webkit_page_shrink_factor;

  cc::PaintCanvas* canvas =
      metafile->GetVectorCanvasForNewPage(page_size, canvas_area, scale_factor);
  if (!canvas)
    return;

  MetafileSkiaWrapper::SetMetafileOnCanvas(canvas, metafile);
  if (params.display_header_footer) {
    PrintHeaderAndFooter(canvas, page_number + 1, page_count, *frame,
                         scale_factor, page_layout_in_points, params);
  }
  RenderPageContent(frame, page_number, canvas_area, content_area, scale_factor,
                    canvas);

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile->FinishPage();
  DCHECK(ret);
}

}  // namespace printing
