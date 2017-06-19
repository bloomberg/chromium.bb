// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_discoverer.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {
namespace {

// Implementation of PrinterDiscoverer interface.
class PrinterDiscovererImpl : public PrinterDiscoverer,
                              public UsbPrinterDetector::Observer {
 public:
  explicit PrinterDiscovererImpl(Profile* profile)
      : detector_observer_(this), profile_(profile), weak_ptr_factory_(this) {
    UsbPrinterDetector* usb_detector =
        UsbPrinterDetectorFactory::GetInstance()->Get(profile);
    DCHECK(usb_detector);
    detector_observer_.Add(usb_detector);
    usb_printers_ = usb_detector->GetPrinters();
  }
  ~PrinterDiscovererImpl() override = default;

  // PrinterDiscoverer interface function.
  void AddObserver(PrinterDiscoverer::Observer* observer) override {
    observer_list_.AddObserver(observer);
    // WrappedOnPrintersFound is a simple wrapper around
    // Observer::OnPrintersFound which lets us safely do the initial
    // OnPrintersFound call to the registered observer.  This wrapper buys us
    // weak_ptr semantics on 'this', and also guards against removal of the
    // observer from the Discoverer before this callback is issued.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PrinterDiscovererImpl::WrappedOnPrintersFound,
                              weak_ptr_factory_.GetWeakPtr(), observer,
                              GetAvailablePrinters()));
  }

  // PrinterDiscoverer interface function.
  void RemoveObserver(PrinterDiscoverer::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // PrinterDetector::Observer interface function.
  void OnAvailableUsbPrintersChanged(
      const std::vector<Printer>& printers) override {
    usb_printers_ = printers;
    std::vector<Printer> all_printers = GetAvailablePrinters();
    for (PrinterDiscoverer::Observer& observer : observer_list_) {
      observer.OnPrintersFound(all_printers);
    }
  }

 private:
  // Wrapper for doing the initial OnPrintersFound call on an observer of this
  // object.
  void WrappedOnPrintersFound(PrinterDiscoverer::Observer* observer,
                              const std::vector<Printer>& printers) {
    if (observer_list_.HasObserver(observer)) {
      observer->OnPrintersFound(printers);
      // Since USB is the only thing we're worried about at the moment, and we
      // don't have to wait for those printers to be scanned, we can just tell
      // the observer the initial scan is done now.  This will change when we're
      // also doing network discovery -- we'll hold off on issuing this callback
      // until the network discovery is done as well.
      observer->OnDiscoveryInitialScanDone(printers.size());
    }
  }

  // Get the current set of discovered printers that are not already known
  // to the user's PrintersManager.
  std::vector<Printer> GetAvailablePrinters() {
    // Only know about usb printers for now.  Eventually we'll add discovered
    // network printers as well.
    std::vector<Printer> ret;
    PrintersManager* printers_manager =
        PrintersManagerFactory::GetForBrowserContext(profile_);
    if (!printers_manager) {
      LOG(WARNING) << "Failing to get available printers because no "
                      "PrintersManager exists.";
      return ret;
    }

    for (const Printer& printer : usb_printers_) {
      if (printers_manager->GetPrinter(printer.id()).get() == nullptr) {
        ret.push_back(printer);
      }
    }
    return ret;
  }

  std::vector<Printer> usb_printers_;
  base::ObserverList<PrinterDiscoverer::Observer> observer_list_;
  ScopedObserver<UsbPrinterDetector, UsbPrinterDetector::Observer>
      detector_observer_;
  Profile* profile_;
  base::WeakPtrFactory<PrinterDiscovererImpl> weak_ptr_factory_;
};

}  // namespace

// static
std::unique_ptr<PrinterDiscoverer> PrinterDiscoverer::CreateForProfile(
    Profile* profile) {
  return base::MakeUnique<PrinterDiscovererImpl>(profile);
}

}  // namespace chromeos
