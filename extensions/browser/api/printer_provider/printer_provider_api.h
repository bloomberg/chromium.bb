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
#include "base/memory/ref_counted_memory.h"
#include "base/scoped_observer.h"
#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
class PrinterProviderInternalAPI;
}

namespace extensions {

// Implements chrome.printerProvider API events.
class PrinterProviderAPI : public BrowserContextKeyedAPI,
                           public PrinterProviderInternalAPIObserver,
                           public ExtensionRegistryObserver {
 public:
  // Struct describing print job that should be forwarded to an extension via
  // chrome.printerProvider.onPrintRequested event.
  struct PrintJob {
    PrintJob();
    ~PrintJob();

    // The id of the printer that should handle the print job. The id is
    // formatted as <extension_id>:<printer_id>, where <extension_id> is the
    // id of the extension that manages the printer, and <printer_id> is
    // the the printer's id within the extension (as reported via
    // chrome.printerProvider.onGetPrintersRequested event callback).
    std::string printer_id;

    // The print job ticket.
    std::string ticket_json;

    // Content type of the document that should be printed.
    std::string content_type;

    // The document data that should be printed.
    scoped_refptr<base::RefCountedMemory> document_bytes;
  };

  using GetPrintersCallback =
      base::Callback<void(const base::ListValue& printers, bool done)>;
  using GetCapabilityCallback =
      base::Callback<void(const base::DictionaryValue& capability)>;
  using PrintCallback =
      base::Callback<void(bool success, const std::string& error)>;

  static BrowserContextKeyedAPIFactory<PrinterProviderAPI>*
  GetFactoryInstance();

  static std::string GetDefaultPrintError();

  explicit PrinterProviderAPI(content::BrowserContext* browser_context);
  ~PrinterProviderAPI() override;

  // Requests list of supported printers from extensions implementing
  // chrome.printerProvider API. It dispatches
  // chrome.printerProvider.onGetPrintersRequested event. The callback is
  // called once for every extension handling the event with a list of its
  // supported printers. The printer values reported by an extension are
  // added "extensionId" property that is set to the ID of the extension
  // returning the list.
  // Note that the "id" property of printer values reported by an extension are
  // rewriten as "<extension_id>:<id>" to ensure they are unique across
  // different extensions.
  void DispatchGetPrintersRequested(const GetPrintersCallback& callback);

  // Requests printer capability for a printer with id |printer_id|.
  // |printer_id| should be one of the printer ids reported by |GetPrinters|
  // callback.
  // It dispatches chrome.printerProvider.onGetCapabilityRequested event
  // to the extension that manages the printer (which can be determined from
  // |printer_id| value).
  // |callback| is passed a dictionary value containing printer capabilities as
  // reported by the extension.
  void DispatchGetCapabilityRequested(const std::string& printer_id,
                                      const GetCapabilityCallback& callback);

  // It dispatches chrome.printerProvider.onPrintRequested event with the
  // provided print job. The event is dispatched only to the extension that
  // manages printer with id |job.printer_id|.
  // |callback| is passed the print status returned by the extension, and it
  // must not be null.
  void DispatchPrintRequested(const PrintJob& job,
                              const PrintCallback& callback);

  // The API currently cannot handle very large files, so the document size that
  // may be sent to an extension is restricted.
  // TODO(tbarzic): Fix the API to support huge documents.
  static const int kMaxDocumentSize = 50 * 1000 * 1000;

 private:
  friend class BrowserContextKeyedAPIFactory<PrinterProviderAPI>;

  // Holds information about a pending onGetPrintersRequested request;
  // in particular, the list of extensions to which the event was dispatched but
  // which haven't yet responded, and the |GetPrinters| callback associated with
  // the event.
  class GetPrintersRequest {
   public:
    explicit GetPrintersRequest(const GetPrintersCallback& callback);
    ~GetPrintersRequest();

    // Adds an extension id to the list of the extensions that need to respond
    // to the event.
    void AddSource(const std::string& extension_id);

    // Whether all extensions have responded to the event.
    bool IsDone() const;

    // Runs the callback for an extension and removes the extension from the
    // list of extensions that still have to respond to the event.
    void ReportForExtension(const std::string& extension_id,
                            const base::ListValue& printers);

   private:
    // Callback reporting event result for an extension. Called once for each
    // extension.
    GetPrintersCallback callback_;

    // The list of extensions that still have to respond to the event.
    std::set<std::string> extensions_;
  };

  // Keeps track of pending chrome.printerProvider.onGetPrintersRequested
  // requests.
  class PendingGetPrintersRequests {
   public:
    PendingGetPrintersRequests();
    ~PendingGetPrintersRequests();

    // Adds a new request to the set of pending requests. Returns the id
    // assigned to the request.
    int Add(const GetPrintersCallback& callback);

    // Completes a request for an extension. It runs the request callback with
    // values reported by the extension.
    bool CompleteForExtension(const std::string& extension_id,
                              int request_id,
                              const base::ListValue& result);

    // Runs callbacks for the extension for all requests that are waiting for a
    // response from the extension with the provided extension id. Callbacks are
    // called as if the extension reported empty set of printers.
    void FailAllForExtension(const std::string& extension_id);

    // Adds an extension id to the list of the extensions that need to respond
    // to the event.
    bool AddSource(int request_id, const std::string& extension_id);

   private:
    int last_request_id_;
    std::map<int, GetPrintersRequest> pending_requests_;

    DISALLOW_COPY_AND_ASSIGN(PendingGetPrintersRequests);
  };

  // Keeps track of pending chrome.printerProvider.onGetCapabilityRequested
  // requests for an extension.
  class PendingGetCapabilityRequests {
   public:
    PendingGetCapabilityRequests();
    ~PendingGetCapabilityRequests();

    // Adds a new request to the set. Only information needed is the callback
    // associated with the request. Returns the id assigned to the request.
    int Add(const GetCapabilityCallback& callback);

    // Completes the request with the provided request id. It runs the request
    // callback and removes the request from the set.
    bool Complete(int request_id, const base::DictionaryValue& result);

    // Runs all pending callbacks with empty capability value and clears the
    // set of pending requests.
    void FailAll();

   private:
    int last_request_id_;
    std::map<int, GetCapabilityCallback> pending_requests_;
  };

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
    bool Complete(int request_id, bool success, const std::string& result);

    // Runs all pending callbacks with ERROR_FAILED and clears the set of
    // pending requests.
    void FailAll();

   private:
    int last_request_id_;
    std::map<int, PrintCallback> pending_requests_;
  };

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceRedirectedInIncognito = true;
  static const char* service_name() { return "PrinterProvider"; }

  // PrinterProviderInternalAPIObserver implementation:
  void OnGetPrintersResult(
      const Extension* extension,
      int request_id,
      const PrinterProviderInternalAPIObserver::PrinterInfoVector& result)
      override;
  void OnGetCapabilityResult(const Extension* extension,
                             int request_id,
                             const base::DictionaryValue& result) override;
  void OnPrintResult(
      const Extension* extension,
      int request_id,
      core_api::printer_provider_internal::PrintError error) override;

  // ExtensionRegistryObserver implementation:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // Called before chrome.printerProvider.onGetPrintersRequested event is
  // dispatched to an extension. It returns whether the extension is interested
  // in the event. If the extension listens to the event, it's added to the set
  // of |request| sources. |request| is |GetPrintersRequest| object associated
  // with the event.
  bool WillRequestPrinters(int request_id,
                           content::BrowserContext* browser_context,
                           const Extension* extension,
                           base::ListValue* args);

  content::BrowserContext* browser_context_;

  PendingGetPrintersRequests pending_get_printers_requests_;

  std::map<std::string, PendingPrintRequests> pending_print_requests_;

  std::map<std::string, PendingGetCapabilityRequests>
      pending_capability_requests_;

  ScopedObserver<PrinterProviderInternalAPI, PrinterProviderInternalAPIObserver>
      internal_api_observer_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    PrinterProviderAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_
