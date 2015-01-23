// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_api.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

static base::LazyInstance<BrowserContextKeyedAPIFactory<PrinterProviderAPI>>
    g_api_factory = LAZY_INSTANCE_INITIALIZER;

PrinterProviderAPI::PrintError APIPrintErrorToInternalType(
    core_api::printer_provider_internal::PrintError error) {
  switch (error) {
    case core_api::printer_provider_internal::PRINT_ERROR_NONE:
      // The PrintError parameter is not set, which implies an error.
      return PrinterProviderAPI::PRINT_ERROR_FAILED;
    case core_api::printer_provider_internal::PRINT_ERROR_OK:
      return PrinterProviderAPI::PRINT_ERROR_NONE;
    case core_api::printer_provider_internal::PRINT_ERROR_FAILED:
      return PrinterProviderAPI::PRINT_ERROR_FAILED;
    case core_api::printer_provider_internal::PRINT_ERROR_INVALID_TICKET:
      return PrinterProviderAPI::PRINT_ERROR_INVALID_TICKET;
    case core_api::printer_provider_internal::PRINT_ERROR_INVALID_DATA:
      return PrinterProviderAPI::PRINT_ERROR_INVALID_DATA;
  }
  return PrinterProviderAPI::PRINT_ERROR_FAILED;
}

}  // namespace

PrinterProviderAPI::PrintJob::PrintJob() {
}

PrinterProviderAPI::PrintJob::~PrintJob() {
}

// static
BrowserContextKeyedAPIFactory<PrinterProviderAPI>*
PrinterProviderAPI::GetFactoryInstance() {
  return g_api_factory.Pointer();
}

PrinterProviderAPI::PrinterProviderAPI(content::BrowserContext* browser_context)
    : browser_context_(browser_context), internal_api_observer_(this) {
  internal_api_observer_.Add(
      PrinterProviderInternalAPI::GetFactoryInstance()->Get(browser_context));
}

PrinterProviderAPI::~PrinterProviderAPI() {
}

void PrinterProviderAPI::DispatchGetPrintersRequested(
    const GetPrintersCallback& callback) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->HasEventListener(
          core_api::printer_provider::OnGetPrintersRequested::kEventName)) {
    callback.Run(base::ListValue(), true /* done */);
    return;
  }

  scoped_ptr<GetPrintersRequest> request(new GetPrintersRequest(callback));
  // |pending_get_printers_requests_| take ownership of |request| which gets
  // NULLed out. Save the pointer before passing it to the requests, as it will
  // be needed later on.
  GetPrintersRequest* raw_request_ptr = request.get();
  int request_id = pending_get_printers_requests_.Add(request.Pass());

  scoped_ptr<base::ListValue> internal_args(new base::ListValue);
  // Request id is not part of the public API, but it will be massaged out in
  // custom bindings.
  internal_args->AppendInteger(request_id);

  scoped_ptr<Event> event(
      new Event(core_api::printer_provider::OnGetPrintersRequested::kEventName,
                internal_args.Pass()));
  // This callback is called synchronously during |BroadcastEvent|, so
  // Unretained is safe. Also, |raw_request_ptr| will stay valid at least until
  // |BroadcastEvent| finishes.
  event->will_dispatch_callback =
      base::Bind(&PrinterProviderAPI::WillRequestPrinters,
                 base::Unretained(this), raw_request_ptr);

  event_router->BroadcastEvent(event.Pass());
}

void PrinterProviderAPI::DispatchGetCapabilityRequested(
    const std::string& extension_id,
    const std::string& printer_id,
    const PrinterProviderAPI::GetCapabilityCallback& callback) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id,
          core_api::printer_provider::OnGetCapabilityRequested::kEventName)) {
    callback.Run(base::DictionaryValue());
    return;
  }

  int request_id = pending_capability_requests_[extension_id].Add(callback);

  scoped_ptr<base::ListValue> internal_args(new base::ListValue);
  // Request id is not part of the public API, but it will be massaged out in
  // custom bindings.
  internal_args->AppendInteger(request_id);
  internal_args->AppendString(printer_id);

  scoped_ptr<Event> event(new Event(
      core_api::printer_provider::OnGetCapabilityRequested::kEventName,
      internal_args.Pass()));

  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

void PrinterProviderAPI::DispatchPrintRequested(
    const std::string& extension_id,
    const PrinterProviderAPI::PrintJob& job,
    const PrinterProviderAPI::PrintCallback& callback) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id,
          core_api::printer_provider::OnPrintRequested::kEventName)) {
    callback.Run(PRINT_ERROR_FAILED);
    return;
  }

  core_api::printer_provider::PrintJob print_job;
  print_job.printer_id = job.printer_id;
  print_job.content_type = job.content_type;
  print_job.document =
      std::vector<char>(job.document_bytes.begin(), job.document_bytes.end());

  int request_id = pending_print_requests_[extension_id].Add(callback);

  scoped_ptr<base::ListValue> internal_args(new base::ListValue);
  // Request id is not part of the public API and it will be massaged out in
  // custom bindings.
  internal_args->AppendInteger(request_id);
  internal_args->Append(print_job.ToValue().release());
  scoped_ptr<Event> event(
      new Event(core_api::printer_provider::OnPrintRequested::kEventName,
                internal_args.Pass()));

  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

PrinterProviderAPI::GetPrintersRequest::GetPrintersRequest(
    const GetPrintersCallback& callback)
    : callback_(callback) {
}

PrinterProviderAPI::GetPrintersRequest::~GetPrintersRequest() {
}

void PrinterProviderAPI::GetPrintersRequest::AddSource(
    const std::string& extension_id) {
  extensions_.insert(extension_id);
}

bool PrinterProviderAPI::GetPrintersRequest::IsDone() const {
  return extensions_.empty();
}

void PrinterProviderAPI::GetPrintersRequest::ReportForExtension(
    const std::string& extension_id,
    const base::ListValue& printers) {
  if (extensions_.erase(extension_id) > 0)
    callback_.Run(printers, IsDone());
}

PrinterProviderAPI::PendingGetPrintersRequests::PendingGetPrintersRequests()
    : last_request_id_(0) {
}

PrinterProviderAPI::PendingGetPrintersRequests::~PendingGetPrintersRequests() {
  STLDeleteContainerPairSecondPointers(pending_requests_.begin(),
                                       pending_requests_.end());
}

int PrinterProviderAPI::PendingGetPrintersRequests::Add(
    scoped_ptr<GetPrintersRequest> request) {
  pending_requests_[++last_request_id_] = request.release();
  return last_request_id_;
}

bool PrinterProviderAPI::PendingGetPrintersRequests::CompleteForExtension(
    const std::string& extension_id,
    int request_id,
    const base::ListValue& result) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  GetPrintersRequest* request = it->second;
  request->ReportForExtension(extension_id, result);
  if (request->IsDone()) {
    pending_requests_.erase(it);
    delete request;
  }
  return true;
}

PrinterProviderAPI::PendingGetCapabilityRequests::PendingGetCapabilityRequests()
    : last_request_id_(0) {
}

PrinterProviderAPI::PendingGetCapabilityRequests::
    ~PendingGetCapabilityRequests() {
}

int PrinterProviderAPI::PendingGetCapabilityRequests::Add(
    const PrinterProviderAPI::GetCapabilityCallback& callback) {
  pending_requests_[++last_request_id_] = callback;
  return last_request_id_;
}

bool PrinterProviderAPI::PendingGetCapabilityRequests::Complete(
    int request_id,
    const base::DictionaryValue& response) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  GetCapabilityCallback callback = it->second;
  pending_requests_.erase(it);

  callback.Run(response);
  return true;
}

PrinterProviderAPI::PendingPrintRequests::PendingPrintRequests()
    : last_request_id_(0) {
}

PrinterProviderAPI::PendingPrintRequests::~PendingPrintRequests() {
}

int PrinterProviderAPI::PendingPrintRequests::Add(
    const PrinterProviderAPI::PrintCallback& callback) {
  pending_requests_[++last_request_id_] = callback;
  return last_request_id_;
}

bool PrinterProviderAPI::PendingPrintRequests::Complete(int request_id,
                                                        PrintError response) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PrintCallback callback = it->second;
  pending_requests_.erase(it);

  callback.Run(response);
  return true;
}

void PrinterProviderAPI::OnGetPrintersResult(const Extension* extension,
                                             int request_id,
                                             const base::ListValue& result) {
  pending_get_printers_requests_.CompleteForExtension(extension->id(),
                                                      request_id, result);
}

void PrinterProviderAPI::OnGetCapabilityResult(
    const Extension* extension,
    int request_id,
    const base::DictionaryValue& result) {
  pending_capability_requests_[extension->id()].Complete(request_id, result);
}

void PrinterProviderAPI::OnPrintResult(
    const Extension* extension,
    int request_id,
    core_api::printer_provider_internal::PrintError error) {
  pending_print_requests_[extension->id()].Complete(
      request_id, APIPrintErrorToInternalType(error));
}

bool PrinterProviderAPI::WillRequestPrinters(
    PrinterProviderAPI::GetPrintersRequest* request,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::ListValue* args) const {
  if (!extension)
    return false;
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension->id(),
          core_api::printer_provider::OnGetPrintersRequested::kEventName)) {
    return false;
  }

  request->AddSource(extension->id());
  return true;
}

template <>
void BrowserContextKeyedAPIFactory<
    PrinterProviderAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(PrinterProviderInternalAPI::GetFactoryInstance());
}

}  // namespace extensions
