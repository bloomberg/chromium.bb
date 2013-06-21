// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PnaclComponentInstaller;

// Monitors the component updater service, so that the callbacks registered
// against the PnaclComponentInstaller can be notified of events.
class PnaclUpdaterObserver : public content::NotificationObserver {
 public:
  explicit PnaclUpdaterObserver(PnaclComponentInstaller* installer);
  virtual ~PnaclUpdaterObserver();

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  PnaclComponentInstaller* pnacl_installer_;
  DISALLOW_COPY_AND_ASSIGN(PnaclUpdaterObserver);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_UPDATER_OBSERVER_H_
