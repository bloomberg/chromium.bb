// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H

namespace chromeos {

namespace tether {

// Scans for nearby tether hosts.
// TODO(khorimoto): Implement.
class HostScanner {
 public:
  HostScanner();
  virtual ~HostScanner();

  virtual void StartScan();
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H
