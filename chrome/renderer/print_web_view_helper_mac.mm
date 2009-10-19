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
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebCanvas.h"
#include "webkit/api/public/WebConsoleMessage.h"

using WebKit::WebFrame;
using WebKit::WebCanvas;
using WebKit::WebConsoleMessage;
using WebKit::WebString;

// TODO(stuartmorgan): There's a fair amount of code here that is duplicated
// from _win that should instead be shared. Once Linux has a real print settings
// implementation, it's likely that this whole method can just be moved to the
// cross-platform file, and the slight divergences resolved/ifdef'd.
void PrintWebViewHelper::Print(WebFrame* frame, bool script_initiated) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 2 * 60;  // 2 Minutes.

  // If still not finished with earlier print request simply ignore.
  if (IsPrinting())
    return;

  // TODO(maruel): Move this out of platform specific code.
  // Check if there is script repeatedly trying to print and ignore it if too
  // frequent.  We use exponential wait time so for a page that calls print() in
  // a loop the user will need to cancel the print dialog after 2 seconds, 4
  // seconds, 8, ... up to the maximum of 2 minutes.
  // This gives the user time to navigate from the page.
  if (script_initiated && (user_cancelled_scripted_print_count_ > 0)) {
    base::TimeDelta diff = base::Time::Now() - last_cancelled_script_print_;
    int min_wait_seconds = std::min(
        kMinSecondsToIgnoreJavascriptInitiatedPrint <<
            (user_cancelled_scripted_print_count_ - 1),
        kMaxSecondsToIgnoreJavascriptInitiatedPrint);
    if (diff.InSeconds() < min_wait_seconds) {
      WebString message(WebString::fromUTF8(
          "Ignoring too frequent calls to print()."));
      frame->addMessageToConsole(WebConsoleMessage(
          WebConsoleMessage::LevelWarning,
          message));
      return;
    }
  }

  // Retrieve the default print settings to calculate the expected number of
  // pages.
  ViewMsg_Print_Params default_settings;
  bool user_cancelled_print = false;

  IPC::SyncMessage* msg =
      new ViewHostMsg_GetDefaultPrintSettings(routing_id(), &default_settings);
  if (Send(msg)) {
    msg = NULL;
    if (default_settings.IsEmpty()) {
      NOTREACHED() << "Couldn't get default print settings";
      return;
    }

    // Continue only if the settings are valid.
    if (default_settings.dpi && default_settings.document_cookie) {
      int expected_pages_count = 0;

      // Prepare once to calculate the estimated page count.  This must be in
      // a scope for itself (see comments on PrepareFrameAndViewForPrint).
      {
        PrepareFrameAndViewForPrint prep_frame_view(default_settings,
                                                    frame,
                                                    frame->view());
        expected_pages_count = prep_frame_view.GetExpectedPageCount();
        DCHECK(expected_pages_count);
      }

      // Ask the browser to show UI to retrieve the final print settings.
      ViewMsg_PrintPages_Params print_settings;

      ViewHostMsg_ScriptedPrint_Params params;

      // The routing id is sent across as it is needed to look up the
      // corresponding RenderViewHost instance to signal and reset the
      // pump messages event.
      params.routing_id = routing_id();
      // host_window_ may be NULL at this point if the current window is a popup
      // and the print() command has been issued from the parent. The receiver
      // of this message has to deal with this.
      params.host_window_id = render_view_->host_window();
      params.cookie = default_settings.document_cookie;
      params.has_selection = frame->hasSelection();
      params.expected_pages_count = expected_pages_count;

      msg = new ViewHostMsg_ScriptedPrint(params, &print_settings);
      if (render_view_->SendAndRunNestedMessageLoop(msg)) {
        msg = NULL;

        // If the settings are invalid, early quit.
        if (print_settings.params.dpi &&
            print_settings.params.document_cookie) {
          if (print_settings.params.selection_only) {
            CopyAndPrint(print_settings, frame);
          } else {
            // TODO: Always copy before printing.
            PrintPages(print_settings, frame);
          }

          // Reset cancel counter on first successful print.
          user_cancelled_scripted_print_count_ = 0;
          return;  // All went well.
        } else {
          user_cancelled_print = true;
        }
      } else {
        // Send() failed.
        NOTREACHED();
      }
    } else {
      // Failed to get default settings.
      NOTREACHED();
    }
  } else {
    // Send() failed.
    NOTREACHED();
  }
  if (script_initiated && user_cancelled_print) {
    ++user_cancelled_scripted_print_count_;
    last_cancelled_script_print_ = base::Time::Now();
  }
  // When |user_cancelled_print| is true, we treat it as success so that
  // DidFinishPrinting() won't show any error alert.
  // If |user_cancelled_print| is false and we reach here, there must be
  // something wrong and hence is not success, DidFinishPrinting() should show
  // an error alert.
  // In both cases, we have to call DidFinishPrinting() here to release
  // printing resources, since we don't need them anymore.
  DidFinishPrinting(user_cancelled_print);
}

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebKit::WebFrame* frame) {
  PrepareFrameAndViewForPrint prep_frame_view(params.params,
                                              frame,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                               params.params.document_cookie,
                                               page_count));
  if (!page_count)
    return;

  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  ViewMsg_PrintPage_Params page_params;
  page_params.params = params.params;
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      page_params.page_number = i;
      PrintPage(page_params, canvas_size, frame);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      page_params.page_number = params.pages[i];
      PrintPage(page_params, canvas_size, frame);
    }
  }
}

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
  unsigned int buf_size = metafile.GetDataSize();
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

