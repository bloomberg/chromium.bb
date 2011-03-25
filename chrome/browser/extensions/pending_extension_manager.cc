// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
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
    const ExtensionUpdateService& service)
    : service_(service) {
}

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

void PendingExtensionManager::AddFromSync(
    const std::string& id,
    const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool install_silently,
    bool enable_on_install,
    bool enable_incognito_on_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (service_.GetExtensionById(id, true)) {
    LOG(DFATAL) << "Trying to add pending extension " << id
                << " which already exists";
    return;
  }

  AddExtensionImpl(id, update_url, should_allow_install, true,
                   install_silently, enable_on_install,
                   enable_incognito_on_install,
                   Extension::INTERNAL);
}

void PendingExtensionManager::AddFromExternalUpdateUrl(
    const std::string& id, const GURL& update_url,
    Extension::Location location) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool kIsFromSync = false;
  const bool kInstallSilently = true;
  const bool kEnableOnInstall = true;
  const bool kEnableIncognitoOnInstall = false;

  if (service_.const_extension_prefs().IsExtensionKilled(id))
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

void PendingExtensionManager::AddExtensionImpl(
    const std::string& id, const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync, bool install_silently,
    bool enable_on_install, bool enable_incognito_on_install,
    Extension::Location install_source) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If a non-sync update is pending, a sync request should not
  // overwrite it.  This is important for external extensions.
  // If an external extension download is pending, and the user has
  // the extension in their sync profile, the install should set the
  // type to be external.  An external extension should not be
  // rejected if it fails the safty checks for a syncable extension.
  // TODO(skerner): Work out other potential overlapping conditions.
  // (crbug.com/61000)

  PendingExtensionInfo pending_extension_info;
  bool has_pending_ext = GetById(id, &pending_extension_info);
  if (has_pending_ext) {
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old is_from_sync = " << pending_extension_info.is_from_sync()
            << "  new is_from_sync = " << is_from_sync;
    if (!pending_extension_info.is_from_sync() && is_from_sync)
      return;
  }

  pending_extension_map_[id] =
      PendingExtensionInfo(update_url,
                           should_allow_install,
                           is_from_sync,
                           install_silently,
                           enable_on_install,
                           enable_incognito_on_install,
                           install_source);
}

void PendingExtensionManager::AddForTesting(
    const std::string& id,
    const PendingExtensionInfo& pending_extension_info) {
  pending_extension_map_[id] = pending_extension_info;
}
