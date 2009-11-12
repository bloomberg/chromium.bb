// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "printing/native_metafile.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"

using WebKit::WebFrame;

void PrintWebViewHelper::Print(WebFrame* frame, bool script_initiated) {
  // If still not finished with earlier print request simply ignore.
  if (IsPrinting())
    return;

  // TODO(myhuang): Get printing parameters via IPC.
  // For testing purpose, we hard-coded printing parameters here.

  // The paper size is US Letter (8.5 in. by 11 in.).
  // Using default margins:
  //   Left = 0.25 in.
  //   Right = 0.25 in.
  //   Top = 0.25 in.
  //   Bottom = 0.56 in.
  const int kDPI = 72;
  const int kWidth = static_cast<int>((8.5-0.25-0.25) * kDPI);
  const int kHeight = static_cast<int>((11-0.25-0.56) * kDPI);
  ViewMsg_Print_Params default_settings;
  default_settings.printable_size = gfx::Size(kWidth, kHeight);
  default_settings.dpi = kDPI;
  default_settings.min_shrink = 1.25;
  default_settings.max_shrink = 2.0;
  default_settings.desired_dpi = kDPI;
  default_settings.document_cookie = 0;
  default_settings.selection_only = false;

  ViewMsg_PrintPages_Params print_settings;
  print_settings.params = default_settings;

  PrintPages(print_settings, frame);
}

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame) {
  PrepareFrameAndViewForPrint prep_frame_view(params.params,
                                              frame,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  // TODO(myhuang): Send ViewHostMsg_DidGetPrintedPagesCount.

  if (page_count == 0)
    return;

  // We only can use PDF in the renderer because Cairo needs to create a
  // temporary file for a PostScript surface.
  printing::NativeMetafile metafile(printing::NativeMetafile::PDF);
  metafile.Init();

  ViewMsg_PrintPage_Params print_page_params;
  print_page_params.params = params.params;
  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      print_page_params.page_number = i;
      PrintPage(print_page_params, canvas_size, frame, &metafile);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      print_page_params.page_number = params.pages[i];
      PrintPage(print_page_params, canvas_size, frame, &metafile);
    }
  }

  metafile.Close();

  // Get the size of the resulting metafile.
  unsigned int buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

  base::FileDescriptor fd;
  int fd_in_browser = -1;

  // Ask the browser to open a file for us.
  if (!Send(new ViewHostMsg_AllocateTempFileForPrinting(&fd,
                                                        &fd_in_browser))) {
    return;
  }

  if (!metafile.SaveTo(fd))
    return;

  // Tell the browser we've finished writing the file.
  Send(new ViewHostMsg_TempFileForPrintingWritten(fd_in_browser));
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame,
                                   printing::NativeMetafile* metafile) {
  // Since WebKit extends the page width depending on the magical shrink
  // factor we make sure the canvas covers the worst case scenario
  // (x2.0 currently).  PrintContext will then set the correct clipping region.
  int size_x = static_cast<int>(canvas_size.width() * params.params.max_shrink);
  int size_y = static_cast<int>(canvas_size.height() *
      params.params.max_shrink);
  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(canvas_size.width()) /
      params.params.printable_size.width();

  cairo_t* cairo_context = metafile->StartPage(size_x, size_y);
  if (!cairo_context) {
    // TODO(myhuang): We should handle such kind of error further!
    // We already have had DLOG(ERROR) in NativeMetafile::StartPage(),
    // log the error here, too?
    return;
  }

  skia::VectorCanvas canvas(cairo_context, size_x, size_y);
  float webkit_shrink = frame->printPage(params.page_number, &canvas);
  if (webkit_shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }

  // TODO(myhuang): We should handle transformation for paper margins.
  // TODO(myhuang): We should render the header and the footer.

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishPage(shrink)) {
    NOTREACHED() << "metafile failed";
  }
}
