// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_

#include "base/observer_list.h"

namespace chromeos {

namespace tether {

// Scans for nearby tether hosts.
class HostScanner {
 public:
  class Observer {
   public:
    virtual void ScanFinished() = 0;

   protected:
    virtual ~Observer() = default;
  };

  HostScanner();
  virtual ~HostScanner();

  // Returns true if a scan is currently in progress.
  virtual bool IsScanActive() = 0;

  // Starts a host scan if there is no current scan. If a scan is active, this
  // function is a no-op.
  virtual void StartScan() = 0;

  // Stops a host scan if there is a current scan. If no scan is active, this
  // function is a no-op.
  virtual void StopScan() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyScanFinished();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
