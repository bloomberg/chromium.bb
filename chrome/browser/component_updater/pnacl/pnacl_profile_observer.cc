// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_profile_observer.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "content/public/browser/notification_service.h"

namespace component_updater {

PnaclProfileObserver::PnaclProfileObserver(
    PnaclComponentInstaller* installer) : pnacl_installer_(installer) {
  // We only need to observe NOTIFICATION_LOGIN_USER_CHANGED for ChromeOS
  // (and it's only defined for ChromeOS).
#if defined(OS_CHROMEOS)
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
#endif
}

PnaclProfileObserver::~PnaclProfileObserver() { }

void PnaclProfileObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    pnacl_installer_->ReRegisterPnacl();
    return;
  }
  NOTREACHED() << "Unexpected notification observed";
#endif
}

}  // namespace component_updater
