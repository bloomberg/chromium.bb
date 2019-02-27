// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_FACTORY_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/printing/external_printers.h"

namespace chromeos {

// Dispenses ExternalPrinters object for device. Access to this object should be
// sequenced.
class DeviceExternalPrintersFactory {
 public:
  static DeviceExternalPrintersFactory* Get();

  // Returns a WeakPtr to the ExternalPrinters registered for the device.
  base::WeakPtr<ExternalPrinters> GetForDevice();

  // Tear down device ExternalPrinters object.
  void Shutdown();

 private:
  friend struct base::LazyInstanceTraitsBase<DeviceExternalPrintersFactory>;

  DeviceExternalPrintersFactory();
  ~DeviceExternalPrintersFactory();

  std::unique_ptr<ExternalPrinters> device_printers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DeviceExternalPrintersFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_FACTORY_H_
