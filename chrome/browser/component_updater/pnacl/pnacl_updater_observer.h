// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/component_updater/component_updater_service.h"

class PnaclComponentInstaller;

// Monitors the component updater service, so that the callbacks registered
// against the PnaclComponentInstaller can be notified of events.
class PnaclUpdaterObserver : public ComponentObserver {
 public:
  explicit PnaclUpdaterObserver(PnaclComponentInstaller* pci);
  virtual ~PnaclUpdaterObserver();

  virtual void OnEvent(Events event, int extra) OVERRIDE;

  void EnsureObserving();
  void StopObserving();

 private:
  bool must_observe_;
  PnaclComponentInstaller* pnacl_installer_;
  DISALLOW_COPY_AND_ASSIGN(PnaclUpdaterObserver);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_
