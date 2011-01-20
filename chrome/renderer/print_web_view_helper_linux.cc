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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"

using printing::NativeMetafile;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebSize;

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node) {
  // We only can use PDF in the renderer because Cairo needs to create a
  // temporary file for a PostScript surface.
  printing::NativeMetafile metafile(printing::NativeMetafile::PDF);
  int page_count;
  skia::VectorCanvas* canvas = NULL;

  {
    // Hack - when |prep_frame_view| goes out of scope, PrintEnd() gets called.
    // Doing this before closing |metafile| below ensures
    // webkit::ppapi::PluginInstance::PrintEnd() has a valid canvas/metafile to
    // save the final output to. See pepper_plugin_instance.cc for the whole
    // story.
    ViewMsg_Print_Params printParams = params.params;
    PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                                frame,
                                                node,
                                                frame->view());
    page_count = prep_frame_view.GetExpectedPageCount();

    // TODO(myhuang): Send ViewHostMsg_DidGetPrintedPagesCount.

    if (page_count == 0)
      return;

    metafile.Init();

    ViewMsg_PrintPage_Params page_params;
    page_params.params = printParams;
    const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
    if (params.pages.empty()) {
      for (int i = 0; i < page_count; ++i) {
        page_params.page_number = i;
        delete canvas;
        PrintPage(page_params, canvas_size, frame, &metafile, &canvas);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        delete canvas;
        PrintPage(page_params, canvas_size, frame, &metafile, &canvas);
      }
    }
  }

  delete canvas;
  metafile.Close();

  int sequence_number = -1;
  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

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
