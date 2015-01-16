// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class PrinterProviderInternalReportPrintResultFunction
    : public UIThreadExtensionFunction {
 public:
  PrinterProviderInternalReportPrintResultFunction();

 protected:
  ~PrinterProviderInternalReportPrintResultFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrintResult",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTRESULT)

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderInternalReportPrintResultFunction);
};

class PrinterProviderInternalReportPrinterCapabilityFunction
    : public UIThreadExtensionFunction {
 public:
  PrinterProviderInternalReportPrinterCapabilityFunction();

 protected:
  ~PrinterProviderInternalReportPrinterCapabilityFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrinterCapability",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTERCAPABILITY)

  DISALLOW_COPY_AND_ASSIGN(
      PrinterProviderInternalReportPrinterCapabilityFunction);
};

class PrinterProviderInternalReportPrintersFunction
    : public UIThreadExtensionFunction {
 public:
  PrinterProviderInternalReportPrintersFunction();

 protected:
  ~PrinterProviderInternalReportPrintersFunction() override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrinters",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTERS)

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderInternalReportPrintersFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_H_
