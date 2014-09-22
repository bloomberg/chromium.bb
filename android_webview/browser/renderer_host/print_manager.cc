// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/print_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/common/print_messages.h"
#include "android_webview/common/render_view_messages.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_settings.h"

using content::BrowserThread;
using printing::PrintSettings;

namespace android_webview {

PrintManager::PrintManager(content::WebContents* contents,
                           PrintSettings* settings,
                           int fd,
                           PrintManagerDelegate* delegate)
    : content::WebContentsObserver(contents),
      settings_(settings),
      fd_(fd),
      delegate_(delegate),
      number_pages_(0),
      cookie_(1),
      printing_(false) {
  DCHECK(delegate_);
}

PrintManager::~PrintManager() {}

bool PrintManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDocumentCookie,
                        OnDidGetDocumentCookie)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintingFailed, OnPrintingFailed)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(PrintHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : WebContentsObserver::OnMessageReceived(message);
}

void PrintManager::OnAllocateTempFileForPrinting(
    base::FileDescriptor* temp_file_fd,
    int* sequence_number) {
  // we don't really use the sequence number.
  *sequence_number = 0;
  temp_file_fd->fd = fd_;
  temp_file_fd->auto_close = false;
}

void PrintManager::OnTempFileForPrintingWritten(int sequence_number) {
  delegate_->DidExportPdf(true);
}

void PrintManager::OnDidGetPrintedPagesCount(int cookie,
                                             int number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0);
  number_pages_ = number_pages;
}

void PrintManager::OnDidGetDocumentCookie(int cookie) {
  cookie_ = cookie;
}

void PrintManager::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  // Unlike the printing_message_filter, we do process this in ui thread
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  OnGetDefaultPrintSettingsReply(reply_msg);
}

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   PrintMsg_Print_Params* params) {
  params->page_size = settings.page_setup_device_units().physical_size();
  params->content_size.SetSize(
      settings.page_setup_device_units().content_area().width(),
      settings.page_setup_device_units().content_area().height());
  params->printable_area.SetRect(
      settings.page_setup_device_units().printable_area().x(),
      settings.page_setup_device_units().printable_area().y(),
      settings.page_setup_device_units().printable_area().width(),
      settings.page_setup_device_units().printable_area().height());
  params->margin_top = settings.page_setup_device_units().content_area().y();
  params->margin_left = settings.page_setup_device_units().content_area().x();
  params->dpi = settings.dpi();
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = settings.min_shrink();
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = settings.max_shrink();
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = settings.desired_dpi();
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only();
  params->supports_alpha_blend = settings.supports_alpha_blend();
  params->should_print_backgrounds = settings.should_print_backgrounds();
  params->display_header_footer = settings.display_header_footer();
  params->title = settings.title();
  params->url = settings.url();
}

void PrintManager::OnGetDefaultPrintSettingsReply(IPC::Message* reply_msg) {
  PrintMsg_Print_Params params;
  RenderParamsFromPrintSettings(*settings_, &params);
  params.document_cookie = cookie_;
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
}

void PrintManager::OnPrintingFailed(int cookie) {
  if (cookie != cookie_) {
    NOTREACHED();
    return;
  }
  delegate_->DidExportPdf(false);
}

bool PrintManager::PrintNow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (printing_)
    return false;

  printing_ = true;
  return Send(new PrintMsg_PrintPages(routing_id()));
}

}  // namespace android_webview
