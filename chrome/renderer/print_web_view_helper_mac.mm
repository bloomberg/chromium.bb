// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#import <AppKit/AppKit.h>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/native_metafile.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"

using WebKit::WebFrame;
using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebSize;

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  printing::NativeMetafile metafile;
  CGContextRef context = metafile.Init();

  float scale_factor = frame->getPrintPageShrink(params.page_number);
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
  metafile.StartPage(content_width_in_points,
                     content_height_in_points,
                     scale_factor);

  // printPage can create autoreleased references to |canvas|. PDF contexts
  // don't write all their data until they are destroyed, so we need to make
  // certain that there are no lingering references.
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  frame->printPage(params.page_number, context);
  [pool release];

  metafile.FinishPage();
  metafile.Close();

  // Get the size of the compiled metafile.
  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = 0;
  page_params.page_number = params.page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  base::SharedMemory shared_buf;

  page_params.page_size = gfx::Size(
      static_cast<int>(content_width_in_points +
                       margin_left_in_points + margin_right_in_points),
      static_cast<int>(content_height_in_points +
                       margin_top_in_points + margin_bottom_in_points));
  page_params.content_area = gfx::Rect(
      static_cast<int>(margin_left_in_points),
      static_cast<int>(margin_top_in_points),
      static_cast<int>(content_width_in_points),
      static_cast<int>(content_height_in_points));

  // Ask the browser to create the shared memory for us.
  uint32 buf_size = metafile.GetDataSize();
  base::SharedMemoryHandle shared_mem_handle;
  if (Send(new ViewHostMsg_AllocatePDFTransport(routing_id(), buf_size,
                                                &shared_mem_handle))) {
    if (base::SharedMemory::IsHandleValid(shared_mem_handle)) {
      base::SharedMemory shared_buf(shared_mem_handle, false);
      if (shared_buf.Map(buf_size)) {
        metafile.GetData(shared_buf.memory(), buf_size);
        page_params.data_size = buf_size;
        shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                                 &(page_params.metafile_data_handle));
      } else {
        NOTREACHED() << "Map failed";
      }
    } else {
      NOTREACHED() << "Browser failed to allocate shared memory";
    }
  } else {
    NOTREACHED() << "Browser allocation request message failed";
  }

  if (!is_preview_)
    Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
}

