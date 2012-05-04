// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Install predicate used by AddFromExternalUpdateUrl().
bool AlwaysInstall(const Extension& extension) {
  return true;
}

}  // namespace

PendingExtensionManager::PendingExtensionManager(
    const ExtensionServiceInterface& service)
    : service_(service) {
}

PendingExtensionManager::~PendingExtensionManager() {}

bool PendingExtensionManager::GetById(
    const std::string& id,
    PendingExtensionInfo* out_pending_extension_info) const {

  PendingExtensionMap::const_iterator it = pending_extension_map_.find(id);
  if (it != pending_extension_map_.end()) {
    *out_pending_extension_info = it->second;
    return true;
  }

  return false;
}

void PendingExtensionManager::Remove(const std::string& id) {
  pending_extension_map_.erase(id);
}

bool PendingExtensionManager::IsIdPending(const std::string& id) const {
  return ContainsKey(pending_extension_map_, id);
}

bool PendingExtensionManager::AddFromSync(
    const std::string& id,
    const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool install_silently) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (service_.GetInstalledExtension(id)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  // Make sure we don't ever try to install the CWS app, because even though
  // it is listed as a syncable app (because its values need to be synced) it
  // should already be installed on every instance.
  if (id == extension_misc::kWebStoreAppId) {
    NOTREACHED();
    return false;
  }

  const bool kIsFromSync = true;
  const Extension::Location kSyncLocation = Extension::INTERNAL;

  return AddExtensionImpl(id, update_url, Version(), should_allow_install,
                          kIsFromSync, install_silently, kSyncLocation);
}

bool PendingExtensionManager::AddFromExternalUpdateUrl(
    const std::string& id, const GURL& update_url,
    Extension::Location location) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool kIsFromSync = false;
  const bool kInstallSilently = true;

  const Extension* extension = service_.GetInstalledExtension(id);
  if (extension &&
      location == Extension::GetHigherPriorityLocation(location,
                                                       extension->location())) {
    // If the new location has higher priority than the location of an existing
    // extension, let the update process overwrite the existing extension.
  } else {
    if (service_.IsExternalExtensionUninstalled(id))
      return false;

    if (extension) {
      LOG(DFATAL) << "Trying to add extension " << id
                  << " by external update, but it is already installed.";
      return false;
    }
  }

  return AddExtensionImpl(id, update_url, Version(), &AlwaysInstall,
                          kIsFromSync, kInstallSilently,
                          location);
}


bool PendingExtensionManager::AddFromExternalFile(
    const std::string& id,
    Extension::Location install_source,
    const Version& version) {
  // TODO(skerner): AddFromSync() checks to see if the extension is
  // installed, but this method assumes that the caller already
  // made sure it is not installed.  Make all AddFrom*() methods
  // consistent.
  GURL kUpdateUrl = GURL();
  bool kIsFromSync = false;
  bool kInstallSilently = true;

  return AddExtensionImpl(
      id,
      kUpdateUrl,
      version,
      &AlwaysInstall,
      kIsFromSync,
      kInstallSilently,
      install_source);
}

void PendingExtensionManager::GetPendingIdsForUpdateCheck(
    std::set<std::string>* out_ids_for_update_check) const {
  PendingExtensionMap::const_iterator iter;
  for (iter = pending_extension_map_.begin();
       iter != pending_extension_map_.end();
       ++iter) {
    Extension::Location install_source = iter->second.install_source();

    // Some install sources read a CRX from the filesystem.  They can
    // not be fetched from an update URL, so don't include them in the
    // set of ids.
    if (install_source == Extension::EXTERNAL_PREF ||
        install_source == Extension::EXTERNAL_REGISTRY)
      continue;

    out_ids_for_update_check->insert(iter->first);
  }
}

bool PendingExtensionManager::AddExtensionImpl(
    const std::string& id,
    const GURL& update_url,
    const Version& version,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync,
    bool install_silently,
    Extension::Location install_source) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingExtensionInfo pending;
  if (GetById(id, &pending)) {
    // Bugs in this code will manifest as sporadic incorrect extension
    // locations in situations where multiple install sources run at the
    // same time. For example, on first login to a chrome os machine, an
    // extension may be requested by sync and the default extension set.
    // The following logging will help diagnose such issues.
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old location: " << pending.install_source()
            << "  new location: " << install_source;

    // Never override an existing extension with an older version. Only
    // extensions from local CRX files have a known version; extensions from an
    // update URL will get the latest version.
    if (version.IsValid() &&
        pending.version().IsValid() &&
        pending.version().CompareTo(version) == 1) {
      VLOG(1) << "Keep existing record (has a newer version).";
      return false;
    }

    Extension::Location higher_priority_location =
        Extension::GetHigherPriorityLocation(
            install_source, pending.install_source());

    if (higher_priority_location != install_source) {
      VLOG(1) << "Keep existing record (has a higher priority location).";
      return false;
    }

    VLOG(1) << "Overwrite existing record.";
  }

  pending_extension_map_[id] = PendingExtensionInfo(
      update_url,
      version,
      should_allow_install,
      is_from_sync,
      install_silently,
      install_source);
  return true;
}

void PendingExtensionManager::AddForTesting(
    const std::string& id,
    const PendingExtensionInfo& pending_extension_info) {
  pending_extension_map_[id] = pending_extension_info;
}
