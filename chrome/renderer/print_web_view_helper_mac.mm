// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "printing/native_metafile_factory.h"
#include "printing/native_metafile.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using WebKit::WebFrame;

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  if (!metafile->Init())
    return;

  float scale_factor = frame->getPrintPageShrink(params.page_number);
  int page_number = params.page_number;

  // Render page for printing.
  gfx::Point origin(0.0f, 0.0f);
  RenderPage(params.params.printable_size, origin, scale_factor, page_number,
      frame, metafile.get());
  metafile->Close();

  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = metafile->GetDataSize();
  page_params.page_number = page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  page_params.page_size = params.params.page_size;
  page_params.content_area = gfx::Rect(params.params.margin_left,
                                       params.params.margin_top,
                                       params.params.printable_size.width(),
                                       params.params.printable_size.height());

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(metafile.get(),
                                   &(page_params.metafile_data_handle))) {
    page_params.data_size = 0;
  }

  Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
}

void PrintWebViewHelper::CreatePreviewDocument(
    const ViewMsg_PrintPages_Params& params, WebKit::WebFrame* frame,
    WebKit::WebNode* node) {
  ViewMsg_Print_Params printParams = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, &printParams);

  PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                              frame, node, frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  if (!page_count)
    return;

  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  if (!metafile->Init())
    return;

  float scale_factor = frame->getPrintPageShrink(0);
  gfx::Point origin(printParams.margin_left, printParams.margin_top);
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      RenderPage(printParams.page_size, origin, scale_factor, i, frame,
          metafile.get());
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      RenderPage(printParams.page_size, origin, scale_factor,
          static_cast<int>(params.pages[i]), frame, metafile.get());
    }
  }
  metafile->Close();

  ViewHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.data_size = metafile->GetDataSize();
  preview_params.document_cookie = params.params.document_cookie;
  preview_params.expected_pages_count = page_count;

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(metafile.get(),
          &(preview_params.metafile_data_handle))) {
    preview_params.data_size = 0;
    preview_params.expected_pages_count = 0;
  }
  Send(new ViewHostMsg_PagesReadyForPreview(routing_id(), preview_params));
}

void PrintWebViewHelper::RenderPage(
    const gfx::Size& page_size, const gfx::Point& content_origin,
    const float& scale_factor, int page_number, WebFrame* frame,
    printing::NativeMetafile* metafile) {
  CGContextRef context = metafile->StartPage(page_size, content_origin,
                                             scale_factor);
  DCHECK(context);

  // printPage can create autoreleased references to |context|. PDF contexts
  // don't write all their data until they are destroyed, so we need to make
  // certain that there are no lingering references.
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  frame->printPage(page_number, context);
  [pool release];

  // Done printing. Close the device context to retrieve the compiled metafile.
  metafile->FinishPage();
}
