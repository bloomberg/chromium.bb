// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/usb_mount_observer.h"

namespace chromeos {

void USBMountObserver::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_CLOSED);
  for (BrowserIterator i = browsers_.begin(); i != browsers_.end();
       ++i) {
    if (Source<Browser>(source).ptr() == i->browser) {
      browsers_.erase(i);
      registrar_.Remove(this,
                        NotificationType::BROWSER_CLOSED,
                        source);
      return;
    }
  }
}

void USBMountObserver::MountChanged(chromeos::MountLibrary* obj,
                                    chromeos::MountEventType evt,
                                    const std::string& path) {
  if (evt == chromeos::DISK_ADDED) {
    // Return since disk added doesn't mean anything until
    // its mounted, which is a change event.
  } else if (evt == chromeos::DISK_REMOVED) {
    RemoveBrowserFromVector(path);
  } else if (evt == chromeos::DISK_CHANGED) {
    BrowserIterator iter = FindBrowserForPath(path);
    if (iter == browsers_.end()) {
      // We don't currently have this one, so it must have been
      // mounted
      const chromeos::MountLibrary::DiskVector& disks = obj->disks();
      for (size_t i = 0; i < disks.size(); ++i) {
        if (disks[i].device_path == path) {
          if (!disks[i].mount_path.empty()) {
            Browser* browser = Browser::CreateForPopup(profile_);
            std::string url = "chrome://filebrowse#";
            url += disks[i].mount_path;
            browser->AddTabWithURL(
                GURL(url), GURL(), PageTransition::START_PAGE,
                true, -1, false, NULL);
            browser->window()->SetBounds(gfx::Rect(0, 0, 250, 300));
            registrar_.Add(this,
                           NotificationType::BROWSER_CLOSED,
                           Source<Browser>(browser));
            browser->window()->Show();
            BrowserWithPath new_browser;
            new_browser.browser = browser;
            new_browser.device_path = disks[i].device_path;
            browsers_.push_back(new_browser);
          }
          return;
        }
      }
    }
  }
}

USBMountObserver::BrowserIterator USBMountObserver::FindBrowserForPath(
    const std::string& path) {
  for (BrowserIterator i = browsers_.begin();i != browsers_.end();
       ++i) {
    if (i->device_path == path) {
      return i;
    }
  }
  return browsers_.end();
}

void USBMountObserver::RemoveBrowserFromVector(const std::string& path) {
  BrowserIterator i = FindBrowserForPath(path);
  if (i != browsers_.end()) {
    registrar_.Remove(this,
                      NotificationType::BROWSER_CLOSED,
                      Source<Browser>(i->browser));
    if (i->browser->window()) {
      i->browser->window()->Close();
    }
    browsers_.erase(i);
  }
}

} // namespace chromeos
