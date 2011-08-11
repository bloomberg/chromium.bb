// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_message_handler.h"

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/shared_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/print_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_restriction.h"
#include "printing/print_job_constants.h"

namespace {

void StopWorker(int document_cookie) {
  printing::PrintJobManager* print_job_manager =
      g_browser_process->print_job_manager();
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager->PopPrinterQuery(document_cookie, &printer_query);
  if (printer_query.get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(printer_query.get(),
                          &printing::PrinterQuery::StopWorker));
  }
}

RefCountedBytes* GetDataFromHandle(base::SharedMemoryHandle handle,
                                   uint32 data_size) {
  scoped_ptr<base::SharedMemory> shared_buf(
      new base::SharedMemory(handle, true));
  if (!shared_buf->Map(data_size)) {
    NOTREACHED();
    return NULL;
  }

  char* preview_data = static_cast<char*>(shared_buf->memory());
  std::vector<unsigned char> data(data_size);
  memcpy(&data[0], preview_data, data_size);
  return RefCountedBytes::TakeVector(&data);
}

}  // namespace

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

void PrintPreviewMessageHandler::OnRequestPrintPreview() {
  PrintPreviewTabController::PrintPreview(tab_contents());
}

void PrintPreviewMessageHandler::OnDidGetPreviewPageCount(int document_cookie,
                                                          int page_count,
                                                          bool is_modifiable) {
  if (page_count <= 0)
    return;

  TabContents* print_preview_tab = GetPrintPreviewTab();
  if (!print_preview_tab || !print_preview_tab->web_ui())
    return;

  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());
  print_preview_ui->OnDidGetPreviewPageCount(
      document_cookie, page_count, is_modifiable);
}

void PrintPreviewMessageHandler::OnDidPreviewPage(
    const PrintHostMsg_DidPreviewPage_Params& params) {
  RenderViewHost* rvh = tab_contents()->render_view_host();
  TabContents* print_preview_tab = GetPrintPreviewTab();
  if (!print_preview_tab || !print_preview_tab->web_ui()) {
    // Can't find print preview tab means we should abort.
    rvh->Send(new PrintMsg_AbortPreview(rvh->routing_id()));
    return;
  }

  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());
  bool has_pending = print_preview_ui->HasPendingRequests();
  if (has_pending) {
    // Cancel. Next print preview request will cancel the current one. Just do
    // the required maintenance work here.
    StopWorker(print_preview_ui->document_cookie());
    print_preview_ui->OnPrintPreviewCancelled();
    return;
  }

  int requested_preview_page_index = INVALID_PAGE_INDEX;
  int page_number = params.page_number;

  if (page_number == FIRST_PAGE_INDEX)
    print_preview_ui->ClearAllPreviewData();

  if (page_number >= FIRST_PAGE_INDEX && params.data_size) {
    RefCountedBytes* data_bytes =
        GetDataFromHandle(params.metafile_data_handle, params.data_size);
    DCHECK(data_bytes);

    print_preview_ui->SetPrintPreviewDataForIndex(page_number, data_bytes);
    print_preview_ui->OnDidPreviewPage(page_number);
    // TODO(kmadhusu): Query |PrintPreviewUI| and update
    // |requested_preview_page_index| accordingly.
  }

  rvh->Send(new PrintMsg_ContinuePreview(rvh->routing_id(),
                                         requested_preview_page_index));
}

void PrintPreviewMessageHandler::OnPagesReadyForPreview(
    const PrintHostMsg_DidPreviewDocument_Params& params) {
  StopWorker(params.document_cookie);

  // Get the print preview tab.
  TabContents* print_preview_tab = GetPrintPreviewTab();
  // User might have closed it already.
  if (!print_preview_tab)
    return;

  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(print_preview_tab);

  if (params.reuse_existing_data) {
    // Need to match normal rendering where we are expected to send this.
    print_preview_ui->OnDidGetPreviewPageCount(params.document_cookie,
                                               params.expected_pages_count,
                                               params.modifiable);

    print_preview_ui->OnReusePreviewData(params.preview_request_id);
    return;
  }

  wrapper->print_view_manager()->OverrideTitle(tab_contents());

  // TODO(joth): This seems like a good match for using RefCountedStaticMemory
  // to avoid the memory copy, but the SetPrintPreviewData call chain below
  // needs updating to accept the RefCountedMemory* base class.
  RefCountedBytes* data_bytes =
      GetDataFromHandle(params.metafile_data_handle, params.data_size);
  if (!data_bytes)
    return;

  print_preview_ui->SetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX,
                                                data_bytes);
  print_preview_ui->OnPreviewDataIsAvailable(
      params.expected_pages_count,
      wrapper->print_view_manager()->RenderSourceName(),
      params.preview_request_id);
}

void PrintPreviewMessageHandler::OnPrintPreviewFailed(int document_cookie) {
  // Always need to stop the worker.
  StopWorker(document_cookie);

  // Inform the print preview tab of the failure.
  TabContents* print_preview_tab = GetPrintPreviewTab();
  // User might have closed it already.
  if (!print_preview_tab)
    return;

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(print_preview_tab);

  if (g_browser_process->background_printing_manager()->
          HasTabContents(wrapper)) {
    // Preview tab was hidden to serve the print request.
    delete wrapper;
  } else {
    PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());
    print_preview_ui->OnPrintPreviewFailed();
  }
}

bool PrintPreviewMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintPreviewMessageHandler, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                        OnRequestPrintPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPreviewPageCount,
                        OnDidGetPreviewPageCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage,
                        OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PagesReadyForPreview,
                        OnPagesReadyForPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintPreviewFailed,
                        OnPrintPreviewFailed)
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
