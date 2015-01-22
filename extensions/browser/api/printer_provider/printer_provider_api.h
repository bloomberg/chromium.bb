// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class PrinterProviderInternalAPI;
}

namespace extensions {

// Implements chrome.printerProvider API events.
class PrinterProviderAPI : public BrowserContextKeyedAPI,
                           public PrinterProviderInternalAPIObserver {
 public:
  // Status returned by chrome.printerProvider.onPrintRequested event.
  enum PrintError {
    PRINT_ERROR_NONE,
    PRINT_ERROR_FAILED,
    PRINT_ERROR_INVALID_TICKET,
    PRINT_ERROR_INVALID_DATA
  };

  // Struct describing print job that should be forwarded to an extension via
  // chrome.printerProvider.onPrintRequested event.
  struct PrintJob {
    PrintJob();
    ~PrintJob();

    // The id of the printer that should handle the print job. This identifies
    // a printer within an extension and is provided by the extension via
    // chrome.printerProvider.onGetPrintersRequested event callback.
    std::string printer_id;

    // The print job ticket.
    std::string ticket_json;

    // Content type of the document that should be printed.
    std::string content_type;

    // The document data that should be printed.
    std::string document_bytes;
  };

  using PrintCallback = base::Callback<void(PrintError)>;

  static BrowserContextKeyedAPIFactory<PrinterProviderAPI>*
  GetFactoryInstance();

  explicit PrinterProviderAPI(content::BrowserContext* browser_context);
  ~PrinterProviderAPI() override;

  // It dispatches chrome.printerProvider.onPrintRequested event to the
  // extension with id |extension_id| with the provided print job.
  // |callback| is passed the print status returned by the extension, and it
  // must not be null.
  void DispatchPrintRequested(const std::string& extension_id,
                              const PrintJob& job,
                              const PrintCallback& callback);

 private:
  friend class BrowserContextKeyedAPIFactory<PrinterProviderAPI>;

  // Keeps track of pending chrome.printerProvider.ontPrintRequested requests
  // for an extension.
  class PendingPrintRequests {
   public:
    PendingPrintRequests();
    ~PendingPrintRequests();

    // Adds a new request to the set. Only information needed is the callback
    // associated with the request. Returns the id assigned to the request.
    int Add(const PrintCallback& callback);

    // Completes the request with the provided request id. It runs the request
    // callback and removes the request from the set.
    bool Complete(int request_id, PrintError result);

   private:
    int last_request_id_;
    std::map<int, PrintCallback> pending_requests_;
  };

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PrinterProvider"; }

  // PrinterProviderInternalAPIObserver implementation:
  void OnPrintResult(
      const Extension* extension,
      int request_id,
      core_api::printer_provider_internal::PrintError error) override;

  content::BrowserContext* browser_context_;

  std::map<std::string, PendingPrintRequests> pending_print_requests_;

  ScopedObserver<PrinterProviderInternalAPI, PrinterProviderInternalAPIObserver>
      internal_api_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    PrinterProviderAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_
