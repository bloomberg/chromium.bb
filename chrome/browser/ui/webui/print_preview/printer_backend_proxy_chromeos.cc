// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

namespace printing {

namespace {

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

}  // namespace printing
