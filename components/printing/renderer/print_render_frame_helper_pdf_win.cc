// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "components/printing/common/print_messages.h"
#include "printing/features/features.h"
#include "printing/metafile_skia_wrapper.h"

namespace printing {

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
bool PrintRenderFrameHelper::PrintPagesNative(blink::WebLocalFrame* frame,
                                              int page_count) {
  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  std::vector<int> printed_pages = GetPrintedPages(params, page_count);
  if (printed_pages.empty())
    return false;

  PdfMetafileSkia metafile(params.params.printed_doc_type);
  CHECK(metafile.Init());

  std::vector<gfx::Size> page_size_in_dpi(printed_pages.size());
  std::vector<gfx::Rect> content_area_in_dpi(printed_pages.size());
  std::vector<gfx::Rect> printable_area_in_dpi(printed_pages.size());
  for (size_t i = 0; i < printed_pages.size(); ++i) {
    PrintPageInternal(params.params, printed_pages[i], page_count, frame,
                      &metafile, &page_size_in_dpi[i], &content_area_in_dpi[i],
                      &printable_area_in_dpi[i]);
  }

  // blink::printEnd() for PDF should be called before metafile is closed.
  FinishFramePrinting();

  metafile.FinishDocument();

  PrintHostMsg_DidPrintPage_Params page_params;
  if (!CopyMetafileDataToSharedMem(metafile,
                                   &page_params.metafile_data_handle)) {
    return false;
  }
  page_params.data_size = metafile.GetDataSize();
  page_params.document_cookie = params.params.document_cookie;

  for (size_t i = 0; i < printed_pages.size(); ++i) {
    page_params.page_number = printed_pages[i];
    page_params.page_size = page_size_in_dpi[i];
    page_params.content_area = content_area_in_dpi[i];
    page_params.physical_offsets =
        gfx::Point(printable_area_in_dpi[i].x(), printable_area_in_dpi[i].y());
    Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
    // Send the rest of the pages with an invalid metafile handle.
    // TODO(erikchen): Fix semantics. See https://crbug.com/640840
    if (page_params.metafile_data_handle.IsValid()) {
      page_params.metafile_data_handle = base::SharedMemoryHandle();
      page_params.data_size = 0;
    }
  }
  return true;
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

}  // namespace printing
