// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_updater_observer.h"

#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

PnaclUpdaterObserver::PnaclUpdaterObserver(PnaclComponentInstaller* pci)
    : must_observe_(false), pnacl_installer_(pci) {}

PnaclUpdaterObserver::~PnaclUpdaterObserver() {}

void PnaclUpdaterObserver::EnsureObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  must_observe_ = true;
}

void PnaclUpdaterObserver::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  must_observe_ = false;
}

void PnaclUpdaterObserver::OnEvent(Events event, int extra) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (must_observe_) {
    switch (event) {
      default:
        break;
      case COMPONENT_UPDATE_READY:
        // If the component updater says there is an UPDATE_READY w/ source
        // being the PNaCl ID, then installation is handed off to the PNaCl
        // installer and we can stop observing. The installer will be the one
        // to run the callback w/ success or failure.
        must_observe_ = false;
        break;
      case COMPONENT_UPDATER_SLEEPING:
        // If the component updater sleeps before an UPDATE_READY for this
        // component, then requests for installs were likely skipped,
        // an error occurred, or there was no new update.
        must_observe_ = false;
        pnacl_installer_->NotifyInstallError();
        break;
    }
  }
}
