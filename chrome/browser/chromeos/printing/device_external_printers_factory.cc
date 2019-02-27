// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/device_external_printers_factory.h"

#include <memory>

#include "base/lazy_instance.h"
#include "chrome/browser/chromeos/printing/external_printers.h"

namespace chromeos {

namespace {

base::LazyInstance<DeviceExternalPrintersFactory>::DestructorAtExit
    g_printers_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
DeviceExternalPrintersFactory* DeviceExternalPrintersFactory::Get() {
  return g_printers_factory.Pointer();
}

base::WeakPtr<ExternalPrinters> DeviceExternalPrintersFactory::GetForDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_printers_)
    device_printers_ = ExternalPrinters::Create();
  return device_printers_->AsWeakPtr();
}

void DeviceExternalPrintersFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_printers_.reset();
}

DeviceExternalPrintersFactory::DeviceExternalPrintersFactory() = default;
DeviceExternalPrintersFactory::~DeviceExternalPrintersFactory() = default;

}  // namespace chromeos
