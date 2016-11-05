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
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
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
  basic_info.printer_name = printer.id();
  basic_info.printer_description = printer.display_name();
  return basic_info;
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
    LOG(WARNING) << "Print setup failed";
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
                        bool success) {
  // It's expected that debug daemon posts callbacks on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HandlePrinterSetup(std::move(printer), success ? SUCCESS : FAILURE, cb);
}

void OnPrinterAddError(const PrinterSetupCallback& cb) {
  // It's expected that debug daemon posts callbacks on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LOG(WARNING) << "Could not contact debugd";
  PostCallbackError(cb);
}

bool IsIppEverywhere(const chromeos::Printer& printer) {
  // TODO(skau): Use uri, effective_make and effective_model to determine if
  // we should do an IPP Everywhere configuration.
  return false;
}

std::string GetPPDPath(const chromeos::Printer& printer) {
  // TODO(skau): Consult the PPD Provider for the correct file path.
  return printer.ppd_reference().user_supplied_ppd_url;
}

}  // namespace

std::string GetDefaultPrinterOnBlockingPoolThread() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  // TODO(crbug.com/660898): Add default printers to ChromeOS.
  return "";
}

void EnumeratePrinters(Profile* profile, const EnumeratePrintersCallback& cb) {
  // PrinterPrefManager is not thread safe and must be called from the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrinterList printer_list;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeCups)) {
    chromeos::PrinterPrefManager* prefs =
        chromeos::PrinterPrefManagerFactory::GetForBrowserContext(profile);
    std::vector<std::unique_ptr<chromeos::Printer>> printers =
        prefs->GetPrinters();
    for (const std::unique_ptr<chromeos::Printer>& printer : printers) {
      printer_list.push_back(ToBasicInfo(*printer));
    }
  }

  cb.Run(printer_list);
}

void ConfigurePrinterAndFetchCapabilities(Profile* profile,
                                          const std::string& printer_name,
                                          const PrinterSetupCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::PrinterPrefManager* prefs =
      chromeos::PrinterPrefManagerFactory::GetForBrowserContext(profile);
  std::unique_ptr<chromeos::Printer> printer = prefs->GetPrinter(printer_name);

  // Check if configuration is viable.
  bool ipp_everywhere = IsIppEverywhere(*printer);
  std::string ppd_path = GetPPDPath(*printer);
  if (!ipp_everywhere && ppd_path.empty()) {
    HandlePrinterSetup(std::move(printer), PPD_NOT_FOUND, cb);
    return;
  }

  // Always push configuration to CUPS.  It may need an update.
  std::string printer_uri = printer->uri();
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->CupsAddPrinter(
      printer_name, printer_uri, ppd_path, ipp_everywhere,
      base::Bind(&OnPrinterAddResult, base::Passed(&printer), cb),
      base::Bind(&OnPrinterAddError, cb));
}

}  // namespace printing
