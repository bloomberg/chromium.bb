// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_PROFILE_OBSERVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_PROFILE_OBSERVER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PnaclComponentInstaller;

// Monitors profile switching for ChromeOS to check the per-user
// version of PNaCl.
class PnaclProfileObserver : public content::NotificationObserver {
 public:
  explicit PnaclProfileObserver(PnaclComponentInstaller* installer);
  virtual ~PnaclProfileObserver();

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  PnaclComponentInstaller* pnacl_installer_;
  DISALLOW_COPY_AND_ASSIGN(PnaclProfileObserver);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_PROFILE_OBSERVER_H_
