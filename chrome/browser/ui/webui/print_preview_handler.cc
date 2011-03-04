// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_handler.h"

#include "base/threading/thread.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/browser_thread.h"
#include "printing/backend/print_backend.h"

class EnumeratePrintersTaskProxy
    : public base::RefCountedThreadSafe<EnumeratePrintersTaskProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  EnumeratePrintersTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
                             printing::PrintBackend* print_backend)
      : handler_(handler),
        print_backend_(print_backend) {
  }

  void EnumeratePrinters() {
    ListValue* printers = new ListValue;

    printing::PrinterList printer_list;
    print_backend_->EnumeratePrinters(&printer_list);
    for (printing::PrinterList::iterator index = printer_list.begin();
         index != printer_list.end(); ++index) {
      printers->Append(new StringValue(index->printer_name));
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &EnumeratePrintersTaskProxy::SendPrinterList,
                          printers));
  }

  void SendPrinterList(ListValue* printers) {
    if (handler_)
      handler_->SendPrinterList(*printers);
    delete printers;
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<EnumeratePrintersTaskProxy>;

  ~EnumeratePrintersTaskProxy() {}

  base::WeakPtr<PrintPreviewHandler> handler_;

  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(EnumeratePrintersTaskProxy);
};

PrintPreviewHandler::PrintPreviewHandler()
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)) {
}

PrintPreviewHandler::~PrintPreviewHandler() {
}

void PrintPreviewHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinters));
  web_ui_->RegisterMessageCallback("print",
      NewCallback(this, &PrintPreviewHandler::HandlePrint));
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue*) {
  scoped_refptr<EnumeratePrintersTaskProxy> task =
      new EnumeratePrintersTaskProxy(AsWeakPtr(), print_backend_.get());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &EnumeratePrintersTaskProxy::EnumeratePrinters));
}

void PrintPreviewHandler::HandlePrint(const ListValue*) {
  web_ui_->GetRenderViewHost()->PrintForPrintPreview();
}

void PrintPreviewHandler::SendPrinterList(const ListValue& printers) {
  web_ui_->CallJavascriptFunction(L"setPrinters", printers);
}
