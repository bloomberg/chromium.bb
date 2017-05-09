// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_web_view_helper.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "components/printing/common/print_messages.h"
#include "printing/features/features.h"
#include "printing/metafile_skia_wrapper.h"

namespace printing {

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
bool PrintWebViewHelper::PrintPagesNative(blink::WebLocalFrame* frame,
                                          int page_count) {
  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  std::vector<int> printed_pages = GetPrintedPages(params, page_count);
  if (printed_pages.empty())
    return false;

  std::vector<gfx::Size> page_size_in_dpi(printed_pages.size());
  std::vector<gfx::Rect> content_area_in_dpi(printed_pages.size());
  std::vector<gfx::Rect> printable_area_in_dpi(printed_pages.size());

  PdfMetafileSkia metafile(PDF_SKIA_DOCUMENT_TYPE);
  CHECK(metafile.Init());

  for (size_t i = 0; i < printed_pages.size(); ++i) {
    PrintPageInternal(params.params, printed_pages[i], page_count, frame,
                      &metafile, &page_size_in_dpi[i], &content_area_in_dpi[i],
                      &printable_area_in_dpi[i]);
  }

  // blink::printEnd() for PDF should be called before metafile is closed.
  FinishFramePrinting();

  metafile.FinishDocument();

  PrintHostMsg_DidPrintPage_Params printed_page_params;
  if (!CopyMetafileDataToSharedMem(
          metafile, &printed_page_params.metafile_data_handle)) {
    return false;
  }

  printed_page_params.content_area = params.params.printable_area;
  printed_page_params.data_size = metafile.GetDataSize();
  printed_page_params.document_cookie = params.params.document_cookie;
  printed_page_params.page_size = params.params.page_size;

  for (size_t i = 0; i < printed_pages.size(); ++i) {
    printed_page_params.page_number = printed_pages[i];
    printed_page_params.page_size = page_size_in_dpi[i];
    printed_page_params.content_area = content_area_in_dpi[i];
    printed_page_params.physical_offsets =
        gfx::Point(printable_area_in_dpi[i].x(), printable_area_in_dpi[i].y());
    Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));
    // Send the rest of the pages with an invalid metafile handle.
    // TODO(erikchen): Fix semantics. See https://crbug.com/640840
    if (printed_page_params.metafile_data_handle.IsValid())
      printed_page_params.metafile_data_handle = base::SharedMemoryHandle();
  }
  return true;
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

}  // namespace printing
