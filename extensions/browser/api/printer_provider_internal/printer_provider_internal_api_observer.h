// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_

#include "extensions/common/api/printer_provider_internal.h"

namespace extensions {

class Extension;

// Interface for observing chrome.printerProviderInternal API function calls.
class PrinterProviderInternalAPIObserver {
 public:
  // Used by chrome.printerProviderInternal API to report
  // chrome.printerProvider.onPrintRequested result returned by the extension
  // |extension|.
  // |request_id| is the request id passed to the original
  // chrome.printerProvider.onPrintRequested event.
  virtual void OnPrintResult(
      const Extension* extension,
      int request_id,
      core_api::printer_provider_internal::PrintError error) = 0;

 protected:
  virtual ~PrinterProviderInternalAPIObserver() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_INTERNAL_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_
