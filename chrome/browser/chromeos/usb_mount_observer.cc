// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb_mount_observer.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/filebrowse_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"


namespace chromeos {

const char* kFilebrowseURLHash = "chrome://filebrowse#";
const char* kFilebrowseScanning = "scanningdevice";
const int kPopupLeft = 0;
const int kPopupTop = 0;
const int kPopupWidth = 250;
const int kPopupHeight = 300;

// static
USBMountObserver* USBMountObserver::GetInstance() {
  return Singleton<USBMountObserver>::get();
}

void USBMountObserver::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_CLOSED);
  for (BrowserIterator i = browsers_.begin(); i != browsers_.end();
       ++i) {
    if (Source<Browser>(source).ptr() == i->browser) {
      i->browser = NULL;
      registrar_.Remove(this,
                        NotificationType::BROWSER_CLOSED,
                        source);
      return;
    }
  }
}

void USBMountObserver::OpenFileBrowse(const std::string& url,
                                      const std::string& device_path,
                                      bool small) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdvancedFileSystem)) {
    return;
  }
  Browser* browser;
  Profile* profile;
  browser =  BrowserList::GetLastActive();
  if (browser == NULL) {
    return;
  }
  profile = browser->profile();
  if (small) {
    browser = FileBrowseUI::OpenPopup(profile,
                                      url,
                                      FileBrowseUI::kSmallPopupWidth,
                                      FileBrowseUI::kSmallPopupHeight);
  } else {
    browser = FileBrowseUI::OpenPopup(profile,
                                      url,
                                      FileBrowseUI::kPopupWidth,
                                      FileBrowseUI::kPopupHeight);
  }

  BrowserIterator iter = FindBrowserForPath(device_path);
  if (iter == browsers_.end()) {
    registrar_.Add(this,
                   NotificationType::BROWSER_CLOSED,
                   Source<Browser>(browser));
    BrowserWithPath new_browser;
    new_browser.browser = browser;
    new_browser.device_path = device_path;
    browsers_.push_back(new_browser);
  } else {
    iter->browser = browser;
  }
}

void USBMountObserver::DiskChanged(MountLibraryEventType event,
                                   const MountLibrary::Disk* disk) {
  if (event == MOUNT_DISK_ADDED) {
    OnDiskAdded(disk);
  } else if (event == MOUNT_DISK_REMOVED) {
    OnDiskRemoved(disk);
  } else if (event == MOUNT_DISK_CHANGED) {
    OnDiskChanged(disk);
  }
}

void USBMountObserver::DeviceChanged(MountLibraryEventType event,
                                     const std::string& device_path) {
  if (event == MOUNT_DEVICE_ADDED) {
    OnDeviceAdded(device_path);
  }else if (event == MOUNT_DEVICE_REMOVED) {
    OnDeviceRemoved(device_path);
  } else if (event == MOUNT_DEVICE_SCANNED) {
    OnDeviceScanned(device_path);
  }
}

void USBMountObserver::FireFileSystemChanged(
    const std::string& web_path) {
  // TODO(zelidrag): Send message to all extensions that file system has
  // changed.
  return;
}

void USBMountObserver::OnDiskAdded(const MountLibrary::Disk* disk) {
  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }
  if (disk->is_parent()) {
    if (!disk->has_media())
      RemoveBrowserFromVector(disk->system_path());
    return;
  }

  // If disk is not mounted yet, give it a try.
  if (disk->mount_path().empty()) {
    // Initiate disk mount operation.
    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->MountPath(disk->device_path().c_str());
  }
}

void USBMountObserver::OnDiskRemoved(const MountLibrary::Disk* disk) {
  VLOG(1) << "Disk removed: " << disk->device_path();
  RemoveBrowserFromVector(disk->system_path());
  MountPointMap::iterator iter = mounted_devices_.find(disk->device_path());
  if (iter == mounted_devices_.end())
    return;

  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  // TODO(zelidrag): This for some reason does not work as advertized.
  // we might need to clean up mount directory on FILE thread here as well.
  lib->UnmountPath(disk->device_path().c_str());

  FireFileSystemChanged(iter->second);
  mounted_devices_.erase(iter);
}

void USBMountObserver::OnDiskChanged(const MountLibrary::Disk* disk) {
  VLOG(1) << "Disk changed : " << disk->device_path();
  if (!disk->mount_path().empty()) {
    // Remember this mount point.
    mounted_devices_.insert(
        std::pair<std::string, std::string>(disk->device_path(),
                                            disk->mount_path()));
    FireFileSystemChanged(disk->mount_path());

    // TODO(zelidrag): We should remove old file browser stuff later.
    // Doing second search to see if the current disk has already
    // been popped up due to its parent device being plugged in.
    BrowserIterator iter = FindBrowserForPath(disk->system_path());
    if (iter != browsers_.end() && iter->browser) {
      std::string url = kFilebrowseURLHash;
      url += disk->mount_path();
      TabContents* tab = iter->browser->GetSelectedTabContents();
      iter->browser->window()->SetBounds(gfx::Rect(
          0, 0, FileBrowseUI::kPopupWidth, FileBrowseUI::kPopupHeight));
      tab->OpenURL(GURL(url), GURL(), CURRENT_TAB,
          PageTransition::LINK);
      tab->NavigateToPendingEntry(NavigationController::RELOAD);
      iter->device_path = disk->device_path();
      iter->mount_path = disk->mount_path();
    } else {
      OpenFileBrowse(disk->mount_path(), disk->device_path(), false);
    }
  }
}

void USBMountObserver::OnDeviceAdded(const std::string& device_path) {
  VLOG(1) << "Device added : " << device_path;
  OpenFileBrowse(kFilebrowseScanning, device_path, true);
}

void USBMountObserver::OnDeviceRemoved(const std::string& system_path) {
  // New device is added, initiate disk rescan.
  RemoveBrowserFromVector(system_path);
}

void USBMountObserver::OnDeviceScanned(const std::string& device_path) {
  VLOG(1) << "Device scanned : " << device_path;
}

USBMountObserver::BrowserIterator USBMountObserver::FindBrowserForPath(
    const std::string& path) {
  for (BrowserIterator i = browsers_.begin();i != browsers_.end();
       ++i) {
    const std::string& device_path = i->device_path;
    // Doing a substring match so that we find if this new one is a subdevice
    // of another already inserted device.
    if (path.find(device_path) != std::string::npos) {
      return i;
    }
  }
  return browsers_.end();
}

void USBMountObserver::RemoveBrowserFromVector(const std::string& system_path) {
  BrowserIterator i = FindBrowserForPath(system_path);
  std::string mount_path;
  if (i != browsers_.end()) {
    registrar_.Remove(this,
                      NotificationType::BROWSER_CLOSED,
                      Source<Browser>(i->browser));
    mount_path = i->mount_path;
    browsers_.erase(i);
  }
  std::vector<Browser*> close_these;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->type() == Browser::TYPE_POPUP) {
      if (*it && (*it)->GetTabContentsAt((*it)->selected_index())) {
        const GURL& url =
            (*it)->GetTabContentsAt((*it)->selected_index())->GetURL();
        if (url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUIFileBrowseHost &&
            url.ref().find(mount_path) != std::string::npos &&
            !mount_path.empty()) {
          close_these.push_back(*it);
        }
      }
    }
  }
  for (size_t x = 0; x < close_these.size(); x++) {
    if (close_these[x]->window()) {
      close_these[x]->window()->Close();
    }
  }
}

} // namespace chromeos
