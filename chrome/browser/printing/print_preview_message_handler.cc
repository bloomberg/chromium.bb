// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_message_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview_ui_html_source.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/print_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace printing {

PrintPreviewMessageHandler::PrintPreviewMessageHandler(
    TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
  DCHECK(tab_contents);
}

PrintPreviewMessageHandler::~PrintPreviewMessageHandler() {
}

TabContents* PrintPreviewMessageHandler::GetPrintPreviewTab() {
  // Get/Create preview tab for initiator tab.
  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return NULL;
  return tab_controller->GetPrintPreviewForTab(tab_contents());
}

void PrintPreviewMessageHandler::OnPagesReadyForPreview(
    const PrintHostMsg_DidPreviewDocument_Params& params) {
  // Always need to stop the worker and send PrintMsg_PrintingDone.
  PrintJobManager* print_job_manager = g_browser_process->print_job_manager();
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager->PopPrinterQuery(params.document_cookie, &printer_query);
  if (printer_query.get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(printer_query.get(),
                          &printing::PrinterQuery::StopWorker));
  }

  RenderViewHost* rvh = tab_contents()->render_view_host();
  rvh->Send(new PrintMsg_PrintingDone(rvh->routing_id(),
                                     params.document_cookie,
                                     true));

  // Get the print preview tab.
  TabContents* print_preview_tab = GetPrintPreviewTab();
  // User might have closed it already.
  if (!print_preview_tab)
    return;

  base::SharedMemory* shared_buf =
      new base::SharedMemory(params.metafile_data_handle, true);
  if (!shared_buf->Map(params.data_size)) {
    NOTREACHED();
    delete shared_buf;
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(print_preview_tab);
  wrapper->print_view_manager()->OverrideTitle(tab_contents());

  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());
  PrintPreviewUIHTMLSource* html_source = print_preview_ui->html_source();
  html_source->SetPrintPreviewData(
      std::make_pair(shared_buf, params.data_size));
  print_preview_ui->PreviewDataIsAvailable(
      params.expected_pages_count,
      wrapper->print_view_manager()->RenderSourceName());
}

void PrintPreviewMessageHandler::OnPrintPreviewNodeUnderContextMenu() {
  PrintPreviewTabController::PrintPreview(tab_contents());
}

void PrintPreviewMessageHandler::OnScriptInitiatedPrintPreview() {
  PrintPreviewTabController::PrintPreview(tab_contents());
}

bool PrintPreviewMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintPreviewMessageHandler, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PagesReadyForPreview,
                        OnPagesReadyForPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintPreviewNodeUnderContextMenu,
                        OnPrintPreviewNodeUnderContextMenu)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ScriptInitiatedPrintPreview,
                        OnScriptInitiatedPrintPreview)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintPreviewMessageHandler::DidStartLoading() {
  if (tab_contents()->delegate() &&
      printing::PrintPreviewTabController::IsPrintPreviewTab(tab_contents())) {
    tab_contents()->SetContentRestrictions(CONTENT_RESTRICTION_PRINT);
  }
}

}  // namespace printing
