// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_native_printers_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

base::WeakPtr<chromeos::BulkPrintersCalculator> GetBulkPrintersCalculator() {
  return chromeos::BulkPrintersCalculatorFactory::Get()->GetForDevice();
}

}  // namespace

DeviceNativePrintersHandler::DeviceNativePrintersHandler(
    PolicyService* policy_service)
    : device_native_printers_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceNativePrinters,
              this)) {}

DeviceNativePrintersHandler::~DeviceNativePrintersHandler() {}

void DeviceNativePrintersHandler::OnDeviceExternalDataSet(
    const std::string& policy) {
  GetBulkPrintersCalculator()->ClearData();
}

void DeviceNativePrintersHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  GetBulkPrintersCalculator()->ClearData();
}

void DeviceNativePrintersHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetBulkPrintersCalculator()->SetData(std::move(data));
}

void DeviceNativePrintersHandler::Shutdown() {
  if (device_native_printers_observer_)
    device_native_printers_observer_.reset();
  chromeos::BulkPrintersCalculatorFactory::Get()->Shutdown();
}

}  // namespace policy
