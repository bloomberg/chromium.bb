// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"

namespace {

// Install predicate used by AddFromDefaultAppList().
bool IsApp(const Extension& extension) {
  return extension.is_app();
}

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
    bool install_silently,
    bool enable_on_install,
    bool enable_incognito_on_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (service_.GetExtensionById(id, true)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  const bool kIsFromSync = true;
  const Extension::Location kSyncLocation = Extension::INTERNAL;

  return AddExtensionImpl(id, update_url, should_allow_install,
                          kIsFromSync, install_silently,
                          enable_on_install,
                          enable_incognito_on_install,
                          kSyncLocation);
}

void PendingExtensionManager::AddFromExternalUpdateUrl(
    const std::string& id, const GURL& update_url,
    Extension::Location location) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool kIsFromSync = false;
  const bool kInstallSilently = true;
  const bool kEnableOnInstall = true;
  const bool kEnableIncognitoOnInstall = false;

  if (service_.IsExternalExtensionUninstalled(id))
    return;

  if (service_.GetExtensionById(id, true)) {
    LOG(DFATAL) << "Trying to add extension " << id
                << " by external update, but it is already installed.";
    return;
  }

  AddExtensionImpl(id, update_url, &AlwaysInstall,
                   kIsFromSync, kInstallSilently,
                   kEnableOnInstall, kEnableIncognitoOnInstall,
                   location);
}


// TODO(akalin): Change DefaultAppList to DefaultExtensionList and
// remove the IsApp() check.
void PendingExtensionManager::AddFromDefaultAppList(
    const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool kIsFromSync = false;
  const bool kInstallSilently = true;
  const bool kEnableOnInstall = true;
  const bool kEnableIncognitoOnInstall = true;

  // This can legitimately happen if the user manually installed one of the
  // default apps before this code ran.
  if (service_.GetExtensionById(id, true))
    return;

  AddExtensionImpl(id, GURL(), &IsApp,
                   kIsFromSync, kInstallSilently,
                   kEnableOnInstall, kEnableIncognitoOnInstall,
                   Extension::INTERNAL);
}

void PendingExtensionManager::AddFromExternalFile(
    const std::string& id,
    Extension::Location location) {

  GURL kUpdateUrl = GURL();
  bool kIsFromSync = false;
  bool kInstallSilently = true;
  bool kEnableOnInstall = true;
  bool kEnableIncognitoOnInstall = false;

  pending_extension_map_[id] =
      PendingExtensionInfo(kUpdateUrl,
                           &AlwaysInstall,
                           kIsFromSync,
                           kInstallSilently,
                           kEnableOnInstall,
                           kEnableIncognitoOnInstall,
                           location);
}

bool PendingExtensionManager::AddExtensionImpl(
    const std::string& id, const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync, bool install_silently,
    bool enable_on_install, bool enable_incognito_on_install,
    Extension::Location install_source) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Will add a pending extension record unless this variable is set to false.
  bool should_add_pending_record = true;

  if (ContainsKey(pending_extension_map_, id)) {
    // Bugs in this code will manifest as sporadic incorrect extension
    // locations in situations where multiple install sources run at the
    // same time. For example, on first login to a chrome os machine, an
    // extension may be requested by sync sync and the default extension set.
    // The following logging will help diagnose such issues.
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old location: " << pending_extension_map_[id].install_source()
            << "  new location: " << install_source;

    Extension::Location higher_priority_location =
        Extension::GetHigherPriorityLocation(
            install_source, pending_extension_map_[id].install_source());

    if (higher_priority_location == install_source) {
      VLOG(1) << "Overwrite existing record.";

    } else {
      VLOG(1) << "Keep existing record.";
      should_add_pending_record = false;
    }
  }

  if (should_add_pending_record) {
    pending_extension_map_[id] = PendingExtensionInfo(
        update_url,
        should_allow_install,
        is_from_sync,
        install_silently,
        enable_on_install,
        enable_incognito_on_install,
        install_source);
    return true;
  }
  return false;
}

void PendingExtensionManager::AddForTesting(
    const std::string& id,
    const PendingExtensionInfo& pending_extension_info) {
  pending_extension_map_[id] = pending_extension_info;
}
