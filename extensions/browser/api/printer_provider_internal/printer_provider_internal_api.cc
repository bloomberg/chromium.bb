// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"

namespace internal_api = extensions::core_api::printer_provider_internal;

namespace extensions {

namespace {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>> g_api_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>*
PrinterProviderInternalAPI::GetFactoryInstance() {
  return g_api_factory.Pointer();
}

PrinterProviderInternalAPI::PrinterProviderInternalAPI(
    content::BrowserContext* browser_context) {
}

PrinterProviderInternalAPI::~PrinterProviderInternalAPI() {
}

void PrinterProviderInternalAPI::AddObserver(
    PrinterProviderInternalAPIObserver* observer) {
  observers_.AddObserver(observer);
}

void PrinterProviderInternalAPI::RemoveObserver(
    PrinterProviderInternalAPIObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PrinterProviderInternalAPI::NotifyGetPrintersResult(
    const Extension* extension,
    int request_id,
    const PrinterProviderInternalAPIObserver::PrinterInfoVector& printers) {
  FOR_EACH_OBSERVER(PrinterProviderInternalAPIObserver, observers_,
                    OnGetPrintersResult(extension, request_id, printers));
}

void PrinterProviderInternalAPI::NotifyGetCapabilityResult(
    const Extension* extension,
    int request_id,
    const base::DictionaryValue& capability) {
  FOR_EACH_OBSERVER(PrinterProviderInternalAPIObserver, observers_,
                    OnGetCapabilityResult(extension, request_id, capability));
}

void PrinterProviderInternalAPI::NotifyPrintResult(
    const Extension* extension,
    int request_id,
    core_api::printer_provider_internal::PrintError error) {
  FOR_EACH_OBSERVER(PrinterProviderInternalAPIObserver, observers_,
                    OnPrintResult(extension, request_id, error));
}

PrinterProviderInternalReportPrintResultFunction::
    PrinterProviderInternalReportPrintResultFunction() {
}

PrinterProviderInternalReportPrintResultFunction::
    ~PrinterProviderInternalReportPrintResultFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintResultFunction::Run() {
  scoped_ptr<internal_api::ReportPrintResult::Params> params(
      internal_api::ReportPrintResult::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrinterProviderInternalAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->NotifyPrintResult(extension(), params->request_id, params->error);
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrinterCapabilityFunction::
    PrinterProviderInternalReportPrinterCapabilityFunction() {
}

PrinterProviderInternalReportPrinterCapabilityFunction::
    ~PrinterProviderInternalReportPrinterCapabilityFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrinterCapabilityFunction::Run() {
  scoped_ptr<internal_api::ReportPrinterCapability::Params> params(
      internal_api::ReportPrinterCapability::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->capability) {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    params->capability->additional_properties);
  } else {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    base::DictionaryValue());
  }
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrintersFunction::
    PrinterProviderInternalReportPrintersFunction() {
}

PrinterProviderInternalReportPrintersFunction::
    ~PrinterProviderInternalReportPrintersFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintersFunction::Run() {
  scoped_ptr<internal_api::ReportPrinters::Params> params(
      internal_api::ReportPrinters::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ListValue printers;
  if (params->printers) {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetPrintersResult(extension(), params->request_id,
                                  *params->printers);
  } else {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetPrintersResult(
            extension(), params->request_id,
            PrinterProviderInternalAPIObserver::PrinterInfoVector());
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
