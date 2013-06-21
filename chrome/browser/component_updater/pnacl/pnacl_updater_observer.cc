// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_updater_observer.h"

#include "base/logging.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

PnaclUpdaterObserver::PnaclUpdaterObserver(
    PnaclComponentInstaller* installer) : pnacl_installer_(installer) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING,
                 content::NotificationService::AllSources());
}

PnaclUpdaterObserver::~PnaclUpdaterObserver() { }

void PnaclUpdaterObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING) {
    // If the component updater sleeps before a NotifyInstallSuccess,
    // then requests for installs were likely skipped, or an error occurred.
    pnacl_installer_->NotifyInstallError();
    return;
  }
}
