// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#import <AppKit/AppKit.h>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_cftyperef.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/native_metafile.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"

using WebKit::WebFrame;
using WebKit::WebCanvas;

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  printing::NativeMetafile metafile;
  CGContextRef context = metafile.Init();

  float scale_factor = frame->getPrintPageShrink(params.page_number);
  metafile.StartPage(canvas_size.width(), canvas_size.height(), scale_factor);

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

  Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
}

