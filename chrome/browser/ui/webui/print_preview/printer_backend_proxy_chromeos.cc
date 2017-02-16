// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"

namespace printing {

namespace {

enum SetupResult { SUCCESS, PPD_NOT_FOUND, FAILURE };

// Store the name used in CUPS, Printer#id in |printer_name|, the description
// as the system_driverinfo option value, and the Printer#display_name in
// the |printer_description| field.  This will match how Mac OS X presents
// printer information.
printing::PrinterBasicInfo ToBasicInfo(const chromeos::Printer& printer) {
  PrinterBasicInfo basic_info;

  // TODO(skau): Unify Mac with the other platforms for display name
  // presentation so I can remove this strange code.
  basic_info.options[kDriverInfoTagName] = printer.description();
  basic_info.options[kCUPSEnterprisePrinter] =
      (printer.source() == chromeos::Printer::SRC_POLICY) ? kValueTrue
                                                          : kValueFalse;
  basic_info.printer_name = printer.id();
  basic_info.printer_description = printer.display_name();
  return basic_info;
}

void AddPrintersToList(
    const std::vector<std::unique_ptr<chromeos::Printer>>& printers,
    PrinterList* list) {
  for (const auto& printer : printers) {
    list->push_back(ToBasicInfo(*printer));
  }
}

void FetchCapabilities(std::unique_ptr<chromeos::Printer> printer,
                       const PrinterSetupCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrinterBasicInfo basic_info = ToBasicInfo(*printer);

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&GetSettingsOnBlockingPool, printer->id(), basic_info), cb);
}

void PostCallbackError(const PrinterSetupCallback& cb) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(cb, nullptr));
}

void HandlePrinterSetup(std::unique_ptr<chromeos::Printer> printer,
                        SetupResult result,
                        const PrinterSetupCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != SetupResult::SUCCESS) {
    LOG(WARNING) << "Print setup failed: " << result;
    // TODO(skau): Open printer settings if this is resolvable.
    PostCallbackError(cb);
    return;
  }

  VLOG(1) << "Printer setup successful for " << printer->id()
          << " fetching properties";

  // fetch settings on the blocking pool and invoke callback.
  FetchCapabilities(std::move(printer), cb);
}

void OnPrinterAddResult(std::unique_ptr<chromeos::Printer> printer,
                        const PrinterSetupCallback& cb,
                        int32_t result_code) {
  // It's expected that debug daemon posts callbacks on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HandlePrinterSetup(std::move(printer), (result_code == 0) ? SUCCESS : FAILURE,
                     cb);
}

void OnPrinterAddError(const PrinterSetupCallback& cb) {
  // It's expected that debug daemon posts callbacks on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LOG(WARNING) << "Could not contact debugd";
  PostCallbackError(cb);
}

void AddPrinter(std::unique_ptr<chromeos::Printer> printer,
                const std::string& ppd_contents,
                const PrinterSetupCallback& cb) {
  // Always push configuration to CUPS.  It may need an update.
  const std::string printer_name = printer->id();
  const std::string printer_uri = printer->uri();

  if (!ppd_contents.empty()) {
    chromeos::DBusThreadManager::Get()
        ->GetDebugDaemonClient()
        ->CupsAddManuallyConfiguredPrinter(
            printer_name, printer_uri, ppd_contents,
            base::Bind(&OnPrinterAddResult, base::Passed(&printer), cb),
            base::Bind(&OnPrinterAddError, cb));
  } else {
    // If we weren't given a ppd to use, we'd better be auto-configurable.
    DCHECK(printer->IsIppEverywhere());
    chromeos::DBusThreadManager::Get()
        ->GetDebugDaemonClient()
        ->CupsAddAutoConfiguredPrinter(
            printer_name, printer_uri,
            base::Bind(&OnPrinterAddResult, base::Passed(&printer), cb),
            base::Bind(&OnPrinterAddError, cb));
  }
}

void PPDResolve(std::unique_ptr<chromeos::Printer> printer,
                const PrinterSetupCallback& cb,
                chromeos::printing::PpdProvider::CallbackResultCode result,
                const std::string& ppd_contents) {
  switch (result) {
    case chromeos::printing::PpdProvider::SUCCESS: {
      DCHECK(!ppd_contents.empty());
      AddPrinter(std::move(printer), ppd_contents, cb);
      break;
    }
    case chromeos::printing::PpdProvider::NOT_FOUND:
      HandlePrinterSetup(std::move(printer), PPD_NOT_FOUND, cb);
      break;
    default:
      HandlePrinterSetup(std::move(printer), FAILURE, cb);
      break;
  }
}

class PrinterBackendProxyChromeos : public PrinterBackendProxy {
 public:
  explicit PrinterBackendProxyChromeos(Profile* profile) {
    prefs_ = chromeos::PrintersManagerFactory::GetForBrowserContext(profile);
    ppd_provider_ = chromeos::printing::CreateProvider(profile);

    // Construct the CupsPrintJobManager to listen for printing events.
    chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  }

  ~PrinterBackendProxyChromeos() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  void GetDefaultPrinter(const DefaultPrinterCallback& cb) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // TODO(crbug.com/660898): Add default printers to ChromeOS.

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(cb, ""));
  };

  void EnumeratePrinters(const EnumeratePrintersCallback& cb) override {
    // PrintersManager is not thread safe and must be called from the UI
    // thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    PrinterList printer_list;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableNativeCups)) {
      AddPrintersToList(prefs_->GetPrinters(), &printer_list);
      AddPrintersToList(prefs_->GetRecommendedPrinters(), &printer_list);
    }

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(cb, printer_list));
  };

  void ConfigurePrinterAndFetchCapabilities(
      const std::string& printer_name,
      const PrinterSetupCallback& cb) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<chromeos::Printer> printer =
        prefs_->GetPrinter(printer_name);
    if (!printer) {
      // If the printer was removed, the lookup will fail.
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       base::Bind(cb, nullptr));
      return;
    }

    if (printer->IsIppEverywhere()) {
      // ChromeOS registers printer by GUID rather than display name.
      AddPrinter(std::move(printer), "", cb);
    } else {
      // Ref taken because printer is moved.
      const chromeos::Printer::PpdReference& ppd_ref = printer->ppd_reference();
      ppd_provider_->ResolvePpd(
          ppd_ref, base::Bind(&PPDResolve, base::Passed(&printer), cb));
    }
  };

 private:
  chromeos::PrintersManager* prefs_;
  scoped_refptr<chromeos::printing::PpdProvider> ppd_provider_;

  DISALLOW_COPY_AND_ASSIGN(PrinterBackendProxyChromeos);
};

}  // namespace

std::unique_ptr<PrinterBackendProxy> PrinterBackendProxy::Create(
    Profile* profile) {
  return base::MakeUnique<PrinterBackendProxyChromeos>(profile);
}

}  // namespace printing
