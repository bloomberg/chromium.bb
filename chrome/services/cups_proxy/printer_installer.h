// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_
#define CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_

#include <cups/cups.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

namespace cups_proxy {

enum class InstallPrinterResult {
  kSuccess = 0,

  // Couldn't find any printers referenced by the incoming request.
  kNoPrintersFound,

  // Referenced printer is unknown to Chrome.
  kUnknownPrinterFound,
  kPrinterInstallationFailure,
};

using InstallPrinterCallback = base::OnceCallback<void(InstallPrinterResult)>;

// This CupsProxyService internal manager ensures that any printers referenced
// by an incoming IPP request are installed into the CUPS daemon prior to
// proxying. This class can be created anywhere, but must be accessed from a
// sequenced context.
class PrinterInstaller {
 public:
  // Factory function.
  static std::unique_ptr<PrinterInstaller> Create(
      base::WeakPtr<chromeos::printing::CupsProxyServiceDelegate> delegate);

  virtual ~PrinterInstaller() = default;

  // Pre-installs any printers required by |ipp| into the CUPS daemon, as
  // needed. |cb| will be run on this instance's sequenced context.
  virtual void InstallPrinterIfNeeded(ipp_t* ipp,
                                      InstallPrinterCallback cb) = 0;
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_
