// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_api.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

static base::LazyInstance<BrowserContextKeyedAPIFactory<PrinterProviderAPI>>
    g_api_factory = LAZY_INSTANCE_INITIALIZER;

// The separator between extension id and the extension's internal printer id
// used when generating a printer id unique across extensions.
const char kPrinterIdSeparator = ':';

// Given an extension ID and an ID of a printer reported by the extension, it
// generates a ID for the printer unique across extensions (assuming that the
// printer id is unique in the extension's space).
std::string GeneratePrinterId(const std::string& extension_id,
                              const std::string& internal_printer_id) {
  std::string result = extension_id;
  result.append(1, kPrinterIdSeparator);
  result.append(internal_printer_id);
  return result;
}

// Parses an ID created using |GeneratePrinterId| to it's components:
// the extension ID and the printer ID internal to the extension.
// Returns whenter the ID was succesfully parsed.
bool ParsePrinterId(const std::string& printer_id,
                    std::string* extension_id,
                    std::string* internal_printer_id) {
  size_t separator = printer_id.find_first_of(kPrinterIdSeparator);
  if (separator == std::string::npos)
    return false;
  *extension_id = printer_id.substr(0, separator);
  *internal_printer_id = printer_id.substr(separator + 1);
  return true;
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

// static
std::string PrinterProviderAPI::GetDefaultPrintError() {
  return core_api::printer_provider_internal::ToString(
      core_api::printer_provider_internal::PRINT_ERROR_FAILED);
}

PrinterProviderAPI::PrinterProviderAPI(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      internal_api_observer_(this),
      extension_registry_observer_(this) {
  internal_api_observer_.Add(
      PrinterProviderInternalAPI::GetFactoryInstance()->Get(browser_context));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
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

  // |pending_get_printers_requests_| take ownership of |request| which gets
  // NULLed out. Save the pointer before passing it to the requests, as it will
  // be needed later on.
  int request_id = pending_get_printers_requests_.Add(callback);

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
                 base::Unretained(this), request_id);

  event_router->BroadcastEvent(event.Pass());
}

void PrinterProviderAPI::DispatchGetCapabilityRequested(
    const std::string& printer_id,
    const PrinterProviderAPI::GetCapabilityCallback& callback) {
  std::string extension_id;
  std::string internal_printer_id;
  if (!ParsePrinterId(printer_id, &extension_id, &internal_printer_id)) {
    callback.Run(base::DictionaryValue());
    return;
  }

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
  internal_args->AppendString(internal_printer_id);

  scoped_ptr<Event> event(new Event(
      core_api::printer_provider::OnGetCapabilityRequested::kEventName,
      internal_args.Pass()));

  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

void PrinterProviderAPI::DispatchPrintRequested(
    const PrinterProviderAPI::PrintJob& job,
    const PrinterProviderAPI::PrintCallback& callback) {
  std::string extension_id;
  std::string internal_printer_id;
  if (!ParsePrinterId(job.printer_id, &extension_id, &internal_printer_id)) {
    callback.Run(false, GetDefaultPrintError());
    return;
  }

  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id,
          core_api::printer_provider::OnPrintRequested::kEventName)) {
    callback.Run(false, GetDefaultPrintError());
    return;
  }

  core_api::printer_provider::PrintJob print_job;
  print_job.printer_id = internal_printer_id;

  JSONStringValueSerializer serializer(job.ticket_json);
  scoped_ptr<base::Value> ticket_value(serializer.Deserialize(NULL, NULL));
  if (!ticket_value ||
      !core_api::printer_provider::PrintJob::Ticket::Populate(
          *ticket_value, &print_job.ticket)) {
    callback.Run(false,
                 core_api::printer_provider::ToString(
                     core_api::printer_provider::PRINT_ERROR_INVALID_TICKET));
    return;
  }

  // TODO(tbarzic): Figure out how to support huge documents.
  if (job.document_bytes->size() > PrinterProviderAPI::kMaxDocumentSize) {
    callback.Run(false,
                 core_api::printer_provider::ToString(
                     core_api::printer_provider::PRINT_ERROR_INVALID_DATA));
    return;
  }

  print_job.content_type = job.content_type;
  print_job.document = std::vector<char>(
      job.document_bytes->front(),
      job.document_bytes->front() + job.document_bytes->size());

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
}

int PrinterProviderAPI::PendingGetPrintersRequests::Add(
    const GetPrintersCallback& callback) {
  pending_requests_.insert(
      std::make_pair(++last_request_id_, GetPrintersRequest(callback)));
  return last_request_id_;
}

bool PrinterProviderAPI::PendingGetPrintersRequests::CompleteForExtension(
    const std::string& extension_id,
    int request_id,
    const base::ListValue& result) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  it->second.ReportForExtension(extension_id, result);
  if (it->second.IsDone()) {
    pending_requests_.erase(it);
  }
  return true;
}

void PrinterProviderAPI::PendingGetPrintersRequests::FailAllForExtension(
    const std::string& extension_id) {
  auto it = pending_requests_.begin();
  while (it != pending_requests_.end()) {
    int request_id = it->first;
    // |it| may get deleted during |CompleteForExtension|, so progress it to the
    // next item before calling the method.
    ++it;
    CompleteForExtension(extension_id, request_id, base::ListValue());
  }
}

bool PrinterProviderAPI::PendingGetPrintersRequests::AddSource(
    int request_id,
    const std::string& extension_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  it->second.AddSource(extension_id);
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

void PrinterProviderAPI::PendingGetCapabilityRequests::FailAll() {
  for (auto& request : pending_requests_)
    request.second.Run(base::DictionaryValue());
  pending_requests_.clear();
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

bool PrinterProviderAPI::PendingPrintRequests::Complete(
    int request_id,
    bool success,
    const std::string& response) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PrintCallback callback = it->second;
  pending_requests_.erase(it);

  callback.Run(success, response);
  return true;
}

void PrinterProviderAPI::PendingPrintRequests::FailAll() {
  for (auto& request : pending_requests_)
    request.second.Run(false, GetDefaultPrintError());
  pending_requests_.clear();
}

void PrinterProviderAPI::OnGetPrintersResult(
    const Extension* extension,
    int request_id,
    const PrinterProviderInternalAPIObserver::PrinterInfoVector& result) {
  base::ListValue printer_list;

  // Update some printer description properties to better identify the extension
  // managing the printer.
  for (size_t i = 0; i < result.size(); ++i) {
    scoped_ptr<base::DictionaryValue> printer(result[i]->ToValue());
    std::string internal_printer_id;
    CHECK(printer->GetString("id", &internal_printer_id));
    printer->SetString("id",
                       GeneratePrinterId(extension->id(), internal_printer_id));
    printer->SetString("extensionId", extension->id());
    printer_list.Append(printer.release());
  }

  pending_get_printers_requests_.CompleteForExtension(extension->id(),
                                                      request_id, printer_list);
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
  const std::string error_str =
      error == core_api::printer_provider_internal::PRINT_ERROR_NONE
          ? GetDefaultPrintError()
          : core_api::printer_provider_internal::ToString(error);
  pending_print_requests_[extension->id()].Complete(
      request_id, error == core_api::printer_provider_internal::PRINT_ERROR_OK,
      error_str);
}

void PrinterProviderAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  pending_get_printers_requests_.FailAllForExtension(extension->id());

  auto print_it = pending_print_requests_.find(extension->id());
  if (print_it != pending_print_requests_.end()) {
    print_it->second.FailAll();
    pending_print_requests_.erase(print_it);
  }

  auto capability_it = pending_capability_requests_.find(extension->id());
  if (capability_it != pending_capability_requests_.end()) {
    capability_it->second.FailAll();
    pending_capability_requests_.erase(capability_it);
  }
}

bool PrinterProviderAPI::WillRequestPrinters(
    int request_id,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::ListValue* args) {
  if (!extension)
    return false;
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension->id(),
          core_api::printer_provider::OnGetPrintersRequested::kEventName)) {
    return false;
  }

  return pending_get_printers_requests_.AddSource(request_id, extension->id());
}

template <>
void BrowserContextKeyedAPIFactory<
    PrinterProviderAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(PrinterProviderInternalAPI::GetFactoryInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

}  // namespace extensions
