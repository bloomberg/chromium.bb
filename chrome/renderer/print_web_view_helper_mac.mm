// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/histogram.h"
#include "chrome/common/print_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/page_size_margins.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

#if defined(USE_SKIA)
#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/platform_device.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#endif

using WebKit::WebFrame;

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame) {
  printing::NativeMetafile metafile;
  if (!metafile.Init())
    return;

  float scale_factor = frame->getPrintPageShrink(params.page_number);
  int page_number = params.page_number;

  // Render page for printing.
  gfx::Rect content_area(params.params.content_size);
  RenderPage(params.params.content_size, content_area, scale_factor,
             page_number, frame, false, &metafile);
  metafile.FinishDocument();

  PrintHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = metafile.GetDataSize();
  page_params.page_number = page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  page_params.page_size = params.params.page_size;
  page_params.content_area = gfx::Rect(params.params.margin_left,
                                       params.params.margin_top,
                                       params.params.content_size.width(),
                                       params.params.content_size.height());

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(&metafile,
                                   &(page_params.metafile_data_handle))) {
    page_params.data_size = 0;
  }

  Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
}

bool PrintWebViewHelper::RenderPreviewPage(int page_number) {
  float scale_factor = print_preview_context_.frame()->getPrintPageShrink(0);
  PrintMsg_Print_Params printParams = print_preview_context_.print_params();
  gfx::Rect content_area(printParams.margin_left, printParams.margin_top,
                         printParams.content_size.width(),
                         printParams.content_size.height());

  scoped_ptr<printing::Metafile> draft_metafile;
  printing::Metafile* initial_render_metafile =
      print_preview_context_.metafile();

#if defined(USE_SKIA)
  bool render_to_draft = print_preview_context_.IsModifiable() &&
                         is_print_ready_metafile_sent_;
#else
  // If the page needs to be in both draft metafile and print ready metafile,
  // we should always render to the draft metafile first and then copy that
  // into the print ready metafile because CG does not allow us to do it in
  // the other order.
  bool render_to_draft = print_preview_context_.IsModifiable() &&
                         print_preview_context_.generate_draft_pages();
#endif

  if (render_to_draft) {
    draft_metafile.reset(new printing::PreviewMetafile());
    if (!draft_metafile->Init()) {
      print_preview_context_.set_error(
          PREVIEW_ERROR_MAC_DRAFT_METAFILE_INIT_FAILED);
      LOG(ERROR) << "Draft PreviewMetafile Init failed";
      return false;
    }
    initial_render_metafile = draft_metafile.get();
  }

  base::TimeTicks begin_time = base::TimeTicks::Now();
  RenderPage(printParams.page_size, content_area, scale_factor, page_number,
             print_preview_context_.frame(), true, initial_render_metafile);
  print_preview_context_.RenderedPreviewPage(
      base::TimeTicks::Now() - begin_time);

  if (draft_metafile.get()) {
    draft_metafile->FinishDocument();
#if !defined(USE_SKIA)
    if (!is_print_ready_metafile_sent_) {
      // With CG, we rendered into a new metafile so we could get it as a draft
      // document.  Now we need to add it to print ready document.  But the
      // document has already been scaled and adjusted for margins, so do a 1:1
      // drawing.
      printing::Metafile* print_ready_metafile =
          print_preview_context_.metafile();
      bool success = print_ready_metafile->StartPage(
          printParams.page_size, gfx::Rect(printParams.page_size), 1.0);
      DCHECK(success);
      // StartPage unconditionally flips the content over, flip it back since it
      // was already flipped in |draft_metafile|.
      CGContextTranslateCTM(print_ready_metafile->context(), 0,
                            printParams.page_size.height());
      CGContextScaleCTM(print_ready_metafile->context(), 1.0, -1.0);
      draft_metafile->RenderPage(1,
                                 print_ready_metafile->context(),
                                 draft_metafile->GetPageBounds(1).ToCGRect(),
                                 false /* shrink_to_fit */,
                                 false /* stretch_to_fit */,
                                 true /* center_horizontally */,
                                 true /* center_vertically */);
      print_ready_metafile->FinishPage();
    }
#endif
  } else {
#if defined(USE_SKIA)
    if (print_preview_context_.IsModifiable() &&
        print_preview_context_.generate_draft_pages()) {
      DCHECK(!draft_metafile.get());
      draft_metafile.reset(
          print_preview_context_.metafile()->GetMetafileForCurrentPage());
    }
#endif
  }
  return PreviewPageRendered(page_number, draft_metafile.get());
}

void PrintWebViewHelper::RenderPage(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor, int page_number, WebFrame* frame,
    bool is_preview, printing::Metafile* metafile) {

  {
#if defined(USE_SKIA)
    SkDevice* device = metafile->StartPageForVectorCanvas(
        page_size, content_area, scale_factor);
    if (!device)
      return;

    SkRefPtr<skia::VectorCanvas> canvas = new skia::VectorCanvas(device);
    canvas->unref();  // SkRefPtr and new both took a reference.
    WebKit::WebCanvas* canvas_ptr = canvas.get();
    printing::MetafileSkiaWrapper::SetMetafileOnCanvas(*canvas, metafile);
    skia::SetIsDraftMode(*canvas, is_print_ready_metafile_sent_);
    skia::SetIsPreviewMetafile(*canvas, is_preview);
#else
    bool success = metafile->StartPage(page_size, content_area, scale_factor);
    DCHECK(success);
    // printPage can create autoreleased references to |context|. PDF contexts
    // don't write all their data until they are destroyed, so we need to make
    // certain that there are no lingering references.
    base::mac::ScopedNSAutoreleasePool pool;
    CGContextRef cgContext = metafile->context();
    CGContextRef canvas_ptr = cgContext;
#endif

    printing::PageSizeMargins page_layout_in_points;
    GetPageSizeAndMarginsInPoints(frame, page_number,
                                  print_pages_params_->params,
                                  &page_layout_in_points);

#if !defined(USE_SKIA)
    // For CoreGraphics, print in the margins before printing in the content
    // area so that we don't spill over. Webkit draws a white background in the
    // content area and this acts as a clip.
    // TODO(aayushkumar): Combine the calls to PrintHeaderAndFooter once we
    // can draw in the margins safely in Skia in any order.
    if (print_pages_params_->params.display_header_footer) {
      PrintHeaderAndFooter(canvas_ptr, page_number + 1,
                           print_preview_context_.total_page_count(),
                           scale_factor, page_layout_in_points,
                           *header_footer_info_);
    }
#endif  // !USE_SKIA

    frame->printPage(page_number, canvas_ptr);

#if defined(USE_SKIA)
    if (print_pages_params_->params.display_header_footer) {
      // |page_number| is 0-based, so 1 is added.
      PrintHeaderAndFooter(canvas_ptr, page_number + 1,
                           print_preview_context_.total_page_count(),
                           scale_factor, page_layout_in_points,
                           *header_footer_info_);
    }
#endif  // defined(USE_SKIA)
  }

  // Done printing. Close the device context to retrieve the compiled metafile.
  metafile->FinishPage();
}
