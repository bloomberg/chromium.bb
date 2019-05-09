// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/automatic_usb_printer_configurer.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "chrome/common/chrome_features.h"

namespace chromeos {
namespace {

bool IsInAutomaticList(const std::string& printer_id,
                       const std::vector<Printer>& automatic_printers) {
  for (const auto& automatic_printer : automatic_printers) {
    if (automatic_printer.id() == printer_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

AutomaticUsbPrinterConfigurer::AutomaticUsbPrinterConfigurer(
    std::unique_ptr<PrinterConfigurer> printer_configurer,
    PrinterInstallationManager* installation_manager)
    : printer_configurer_(std::move(printer_configurer)),
      installation_manager_(installation_manager),
      weak_factory_(this) {
  DCHECK(installation_manager);
}

AutomaticUsbPrinterConfigurer::~AutomaticUsbPrinterConfigurer() = default;

void AutomaticUsbPrinterConfigurer::OnPrintersChanged(
    PrinterClass printer_class,
    const std::vector<Printer>& printers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  if (!base::FeatureList::IsEnabled(features::kStreamlinedUsbPrinterSetup)) {
    return;
  }

  if (printer_class != PrinterClass::kAutomatic) {
    return;
  }

  PruneRemovedPrinters(printers);

  for (const Printer& printer : printers) {
    if (!printers_.contains(printer.id()) && printer.IsUsbProtocol()) {
      SetupPrinter(printer);
    }
  }
}

void AutomaticUsbPrinterConfigurer::SetupPrinter(const Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  if (installation_manager_->IsPrinterInstalled(printer)) {
    // Skip setup if the printer is already installed.
    CompleteConfiguration(printer);
  }

  printer_configurer_->SetUpPrinter(
      printer, base::BindOnce(&AutomaticUsbPrinterConfigurer::OnSetupComplete,
                              weak_factory_.GetWeakPtr(), printer));
}

void AutomaticUsbPrinterConfigurer::OnSetupComplete(const Printer& printer,
                                                    PrinterSetupResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  if (result != PrinterSetupResult::kSuccess) {
    LOG(ERROR) << "Unable to autoconfigure usb printer " << printer.id();
    return;
  }
  installation_manager_->PrinterInstalled(printer, true /* is_automatic */);
  CompleteConfiguration(printer);
}

void AutomaticUsbPrinterConfigurer::CompleteConfiguration(
    const Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  VLOG(1) << "Auto USB Printer setup successful for " << printer.id();

  printers_.insert(printer.id());
}

void AutomaticUsbPrinterConfigurer::PruneRemovedPrinters(
    const std::vector<Printer>& automatic_printers) {
  for (auto it = printers_.begin(); it != printers_.end();) {
    if (!IsInAutomaticList(*it, automatic_printers)) {
      it = printers_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace chromeos
