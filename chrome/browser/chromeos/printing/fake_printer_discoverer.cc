// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/fake_printer_discoverer.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/printer_discoverer.h"

namespace chromeos {

// static
std::unique_ptr<PrinterDiscoverer> PrinterDiscoverer::Create() {
  return base::MakeUnique<FakePrinterDiscoverer>();
}

FakePrinterDiscoverer::~FakePrinterDiscoverer() {}

bool FakePrinterDiscoverer::StartDiscovery() {
  discovery_running_ = true;

  for (auto* observer : observers_)
    observer->OnDiscoveryStarted();

  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE, base::Bind(&FakePrinterDiscoverer::EmitPrinters,
                            weak_ptr_factory_.GetWeakPtr(), 0, 2),
      base::TimeDelta::FromMilliseconds(2000));
  return true;
}

bool FakePrinterDiscoverer::StopDiscovery() {
  discovery_running_ = false;
  for (auto* observer : observers_)
    observer->OnDiscoveryStopping();

  return true;
}

void FakePrinterDiscoverer::AddObserver(PrinterDiscoverer::Observer* observer) {
  auto found = std::find(observers_.begin(), observers_.end(), observer);
  if (found == observers_.end())
    observers_.push_back(observer);
}

void FakePrinterDiscoverer::RemoveObserver(
    PrinterDiscoverer::Observer* observer) {
  auto found = std::find(observers_.begin(), observers_.end(), observer);
  if (found != observers_.end())
    observers_.erase(found);
}

FakePrinterDiscoverer::FakePrinterDiscoverer()
    : discovery_running_(false), weak_ptr_factory_(this) {
  for (int i = 0; i < 12; i++) {
    // printer doesn't have a ppd
    printers_.emplace_back(base::StringPrintf("GUID%2d", i));
    printers_[i].set_display_name(base::StringPrintf("PrinterName%2d", i));
    printers_[i].set_description(
        base::StringPrintf("Printer%2dDescription", i));
    printers_[i].set_manufacturer("Chromium");
    printers_[i].set_model(i % 3 == 0 ? "Inkjet" : "Laser Maker");
    printers_[i].set_uri(
        base::StringPrintf("lpd://192.168.1.%d:9100/bldg/printer", i));
    printers_[i].set_uuid(
        base::StringPrintf("UUID-%4d-%4d-%4d-UUID", i * 3, i * 2, i));
  }
}

void FakePrinterDiscoverer::EmitPrinters(size_t start, size_t end) {
  if (!discovery_running_ || start >= printers_.size() || end < start)
    return;

  size_t clipped_start = std::max(static_cast<size_t>(0), start);
  size_t clipped_end = std::min(printers_.size(), end);
  if (!observers_.empty()) {
    std::vector<Printer> subset(printers_.cbegin() + clipped_start,
                                printers_.cbegin() + clipped_end);

    for (auto* observer : observers_)
      observer->OnPrintersFound(subset);
  }

  // schedule another emit if elements remain.
  if (end < printers_.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
        FROM_HERE,
        base::Bind(&FakePrinterDiscoverer::EmitPrinters,
                   weak_ptr_factory_.GetWeakPtr(), clipped_end, end + end),
        base::TimeDelta::FromMilliseconds(2000));
  } else {
    // We're done.  Notify observers.
    discovery_running_ = false;
    for (auto* observer : observers_)
      observer->OnDiscoveryDone();
  }
}

}  // namespace chromeos
