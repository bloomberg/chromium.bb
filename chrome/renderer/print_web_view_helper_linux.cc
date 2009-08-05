// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "skia/ext/vector_canvas.h"
#include "webkit/glue/webframe.h"


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
  const int kWidth = (8.5-0.25-0.25) * kDPI;
  const int kHeight = (11-0.25-0.56) * kDPI;
  ViewMsg_Print_Params default_settings;
  default_settings.printable_size = gfx::Size(kWidth, kHeight);
  default_settings.dpi = kDPI;
  default_settings.min_shrink = 1.25;
  default_settings.max_shrink = 2.0;
  default_settings.desired_dpi = kDPI;
  default_settings.document_cookie = NULL;
  default_settings.selection_only = false;

  // Calculate the estimated page count.
  PrepareFrameAndViewForPrint prep_frame_view(default_settings,
                                              frame,
                                              frame->GetView());
  int expected_pages_count = prep_frame_view.GetExpectedPageCount();
  DCHECK(expected_pages_count);

  ViewMsg_PrintPage_Params page_params;
  page_params.params = default_settings;

  // TODO(myhuang): Get final printing settings via IPC.
  // For testing purpose, we hard-coded printing settings here.

  // Print the first page only.
  expected_pages_count = 1;
  for (int i = 0; i < expected_pages_count; ++i) {
    page_params.page_number = i;
    PrintPage(page_params, prep_frame_view.GetPrintCanvasSize(), frame);
  }
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  // Since WebKit extends the page width depending on the magical shrink
  // factor we make sure the canvas covers the worst case scenario
  // (x2.0 currently).  PrintContext will then set the correct clipping region.
  int size_x = static_cast<int>(canvas_size.width() * params.params.max_shrink);
  int size_y = static_cast<int>(canvas_size.height() *
      params.params.max_shrink);
  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(canvas_size.width()) /
      params.params.printable_size.width();

  // TODO(myhuang): We now use VectorCanvas to generate a PS/PDF file for
  // each page in printing. We might also need to create a metafile class
  // on Linux.
  skia::VectorCanvas canvas(size_x, size_y);
  float webkit_shrink = frame->PrintPage(params.page_number, &canvas);
  if (shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }
}

