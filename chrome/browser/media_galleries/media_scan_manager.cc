// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_scan_manager.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/media_galleries/media_scan_manager_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "content/public/browser/browser_thread.h"

namespace media_galleries = extensions::api::media_galleries;

MediaScanManager::MediaScanManager() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

MediaScanManager::~MediaScanManager() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void MediaScanManager::AddObserver(Profile* profile,
                                   MediaScanManagerObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(observers_.find(profile) == observers_.end());
  observers_[profile] = observer;
}

void MediaScanManager::RemoveObserver(Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  observers_.erase(profile);
}

void MediaScanManager::CancelScansForProfile(Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scanning_extensions_.erase(profile);

  if (scanning_extensions_.empty()) {
    // TODO(tommycli): Stop the physical scan here.
  }
}

void MediaScanManager::StartScan(Profile* profile,
                                 const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(observers_.find(profile) != observers_.end());

  // Ignore requests for extensions that are already scanning.
  ScanningExtensionsMap::iterator existing_profile_extensions =
      scanning_extensions_.find(profile);
  if (existing_profile_extensions != scanning_extensions_.end() &&
      existing_profile_extensions->second.find(extension_id) !=
          existing_profile_extensions->second.end()) {
    return;
  }

  if (scanning_extensions_.empty()) {
    // TODO(tommycli): Start the physical scan here.
  }

  scanning_extensions_[profile].insert(extension_id);

  ObserverMap::iterator observer = observers_.find(profile);
  observer->second->OnScanStarted(extension_id);
}

void MediaScanManager::CancelScan(Profile* profile,
                                  const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ignore requests for extensions that are NOT already scanning.
  ScanningExtensionsMap::iterator profile_extensions =
      scanning_extensions_.find(profile);
  if (profile_extensions == scanning_extensions_.end() ||
      profile_extensions->second.find(extension_id) ==
          profile_extensions->second.end()) {
    return;
  }

  profile_extensions->second.erase(extension_id);
  if (profile_extensions->second.empty())
    scanning_extensions_.erase(profile_extensions);

  if (scanning_extensions_.empty()) {
    // TODO(tommycli): Stop the physical scan here.
  }

  ObserverMap::iterator observer = observers_.find(profile);
  if (observer != observers_.end())
    observer->second->OnScanCancelled(extension_id);
}
