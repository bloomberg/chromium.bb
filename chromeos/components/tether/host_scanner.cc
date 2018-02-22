// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

namespace chromeos {

namespace tether {

HostScanner::HostScanner() = default;

HostScanner::~HostScanner() = default;

void HostScanner::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostScanner::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostScanner::NotifyScanFinished() {
  for (auto& observer : observer_list_)
    observer.ScanFinished();
}

}  // namespace tether

}  // namespace chromeos
