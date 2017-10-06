// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"

namespace {

using chromeos::CupsPrintersManager;

// Store the name used in CUPS, Printer#id in |printer_name|, the description
// as the system_driverinfo option value, and the Printer#display_name in
// the |printer_description| field.  This will match how Mac OS X presents
// printer information.
printing::PrinterBasicInfo ToBasicInfo(const chromeos::Printer& printer) {
  printing::PrinterBasicInfo basic_info;

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

void AddPrintersToList(const std::vector<chromeos::Printer>& printers,
                       printing::PrinterList* list) {
  for (const auto& printer : printers) {
    list->push_back(ToBasicInfo(printer));
  }
}

void FetchCapabilities(std::unique_ptr<chromeos::Printer> printer,
                       const PrinterHandler::GetCapabilityCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrinterBasicInfo basic_info = ToBasicInfo(*printer);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&printing::GetSettingsOnBlockingPool, printer->id(),
                 basic_info),
      cb);
}

}  // namespace

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(Profile* profile)
    : printers_manager_(CupsPrintersManager::Create(profile)),
      printer_configurer_(chromeos::PrinterConfigurer::Create(profile)),
      weak_factory_(this) {
  printers_manager_->Start();

  // Construct the CupsPrintJobManager to listen for printing events.
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalPrinterHandlerChromeos::Reset() {
  weak_factory_.InvalidateWeakPtrs();
}

void LocalPrinterHandlerChromeos::GetDefaultPrinter(
    const DefaultPrinterCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/660898): Add default printers to ChromeOS.

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(cb, ""));
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    const AddedPrintersCallback& added_printers_callback,
    const GetPrintersDoneCallback& done_callback) {
  // SyncedPrintersManager is not thread safe and must be called from the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrinterList printer_list;
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kConfigured),
      &printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kEnterprise),
      &printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kAutomatic),
      &printer_list);

  printing::ConvertPrinterListForCallback(added_printers_callback,
                                          done_callback, printer_list);
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& printer_name,
    const GetCapabilityCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_name);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(cb, nullptr));
    return;
  }

  // Log printer configuration for selected printer.
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.ProtocolUsed",
                            printer->GetProtocol(),
                            chromeos::Printer::kProtocolMax);

  if (printers_manager_->IsPrinterInstalled(*printer)) {
    // Skip setup if the printer is already installed.
    HandlePrinterSetup(std::move(printer), cb, chromeos::kSuccess);
    return;
  }

  const chromeos::Printer& printer_ref = *printer;
  printer_configurer_->SetUpPrinter(
      printer_ref,
      base::Bind(&LocalPrinterHandlerChromeos::HandlePrinterSetup,
                 weak_factory_.GetWeakPtr(), base::Passed(&printer), cb));
}

void LocalPrinterHandlerChromeos::StartPrint(
    const std::string& destination_id,
    const std::string& capability,
    const base::string16& job_title,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedBytes>& print_data,
    const PrintCallback& callback) {
  NOTREACHED();
}

void LocalPrinterHandlerChromeos::HandlePrinterSetup(
    std::unique_ptr<chromeos::Printer> printer,
    const GetCapabilityCallback& cb,
    chromeos::PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (result) {
    case chromeos::PrinterSetupResult::kSuccess:
      VLOG(1) << "Printer setup successful for " << printer->id()
              << " fetching properties";
      printers_manager_->PrinterInstalled(*printer);

      // fetch settings on the blocking pool and invoke callback.
      FetchCapabilities(std::move(printer), cb);
      return;
    case chromeos::PrinterSetupResult::kPpdNotFound:
      LOG(WARNING) << "Could not find PPD.  Check printer configuration.";
      // Prompt user to update configuration.
      // TODO(skau): Fill me in
      break;
    case chromeos::PrinterSetupResult::kPpdUnretrievable:
      LOG(WARNING) << "Could not download PPD.  Check Internet connection.";
      // Could not download PPD.  Connect to Internet.
      // TODO(skau): Fill me in
      break;
    case chromeos::PrinterSetupResult::kPrinterUnreachable:
    case chromeos::PrinterSetupResult::kDbusError:
    case chromeos::PrinterSetupResult::kPpdTooLarge:
    case chromeos::PrinterSetupResult::kInvalidPpd:
    case chromeos::PrinterSetupResult::kFatalError:
      LOG(ERROR) << "Unexpected error in printer setup." << result;
      break;
    case chromeos::PrinterSetupResult::kMaxValue:
      NOTREACHED() << "This value is not expected";
      break;
  }

  // TODO(skau): Open printer settings if this is resolvable.
  cb.Run(nullptr);
}
