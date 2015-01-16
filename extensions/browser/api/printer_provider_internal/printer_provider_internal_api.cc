// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api.h"

#include "extensions/common/api/printer_provider_internal.h"

namespace internal_api = extensions::core_api::printer_provider_internal;

namespace extensions {

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

  return RespondNow(NoArguments());
}

}  // namespace extensions
