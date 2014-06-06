// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/info_map.h"

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;

namespace extensions {

namespace {

void CheckOnValidThread() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

}  // namespace

struct InfoMap::ExtraData {
  // When the extension was installed.
  base::Time install_time;

  // True if the user has allowed this extension to run in incognito mode.
  bool incognito_enabled;

  // True if the user has disabled notifications for this extension manually.
  bool notifications_disabled;

  ExtraData();
  ~ExtraData();
};

InfoMap::ExtraData::ExtraData() : incognito_enabled(false) {}

InfoMap::ExtraData::~ExtraData() {}

InfoMap::InfoMap() : signin_process_id_(-1) {}

const ProcessMap& InfoMap::process_map() const { return process_map_; }

const ProcessMap& InfoMap::worker_process_map() const {
  return worker_process_map_;
}

void InfoMap::AddExtension(const Extension* extension,
                           base::Time install_time,
                           bool incognito_enabled,
                           bool notifications_disabled) {
  CheckOnValidThread();
  extensions_.Insert(extension);
  disabled_extensions_.Remove(extension->id());

  extra_data_[extension->id()].install_time = install_time;
  extra_data_[extension->id()].incognito_enabled = incognito_enabled;
  extra_data_[extension->id()].notifications_disabled = notifications_disabled;
}

void InfoMap::RemoveExtension(const std::string& extension_id,
                              const UnloadedExtensionInfo::Reason reason) {
  CheckOnValidThread();
  const Extension* extension = extensions_.GetByID(extension_id);
  extra_data_.erase(extension_id);  // we don't care about disabled extra data
  bool was_uninstalled = (reason != UnloadedExtensionInfo::REASON_DISABLE &&
                          reason != UnloadedExtensionInfo::REASON_TERMINATE);
  if (extension) {
    if (!was_uninstalled)
      disabled_extensions_.Insert(extension);
    extensions_.Remove(extension_id);
  } else if (was_uninstalled) {
    // If the extension was uninstalled, make sure it's removed from the map of
    // disabled extensions.
    disabled_extensions_.Remove(extension_id);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << extension_id;
  }
}

base::Time InfoMap::GetInstallTime(const std::string& extension_id) const {
  ExtraDataMap::const_iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.install_time;
  return base::Time();
}

bool InfoMap::IsIncognitoEnabled(const std::string& extension_id) const {
  // Keep in sync with duplicate in extensions/browser/process_manager.cc.
  ExtraDataMap::const_iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.incognito_enabled;
  return false;
}

bool InfoMap::CanCrossIncognito(const Extension* extension) const {
  // This is duplicated from ExtensionService :(.
  return IsIncognitoEnabled(extension->id()) &&
         !IncognitoInfo::IsSplitMode(extension);
}

void InfoMap::RegisterExtensionProcess(const std::string& extension_id,
                                       int process_id,
                                       int site_instance_id) {
  if (!process_map_.Insert(extension_id, process_id, site_instance_id)) {
    NOTREACHED() << "Duplicate extension process registration for: "
                 << extension_id << "," << process_id << ".";
  }
}

void InfoMap::UnregisterExtensionProcess(const std::string& extension_id,
                                         int process_id,
                                         int site_instance_id) {
  if (!process_map_.Remove(extension_id, process_id, site_instance_id)) {
    NOTREACHED() << "Unknown extension process registration for: "
                 << extension_id << "," << process_id << ".";
  }
}

void InfoMap::UnregisterAllExtensionsInProcess(int process_id) {
  process_map_.RemoveAllFromProcess(process_id);
}

void InfoMap::RegisterExtensionWorkerProcess(const std::string& extension_id,
                                             int process_id,
                                             int site_instance_id) {
  if (!worker_process_map_.Insert(extension_id, process_id, site_instance_id)) {
    NOTREACHED() << "Duplicate extension worker process registration for: "
                 << extension_id << "," << process_id << ".";
  }
}

void InfoMap::UnregisterExtensionWorkerProcess(int process_id) {
  worker_process_map_.RemoveAllFromProcess(process_id);
}

void InfoMap::GetExtensionsWithAPIPermissionForSecurityOrigin(
    const GURL& origin,
    int process_id,
    APIPermission::ID permission,
    ExtensionSet* extensions) const {
  DCHECK(extensions);

  if (origin.SchemeIs(kExtensionScheme)) {
    const std::string& id = origin.host();
    const Extension* extension = extensions_.GetByID(id);
    if (extension &&
        extension->permissions_data()->HasAPIPermission(permission) &&
        process_map_.Contains(id, process_id)) {
      extensions->Insert(extension);
    }
    return;
  }

  ExtensionSet::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if ((*i)->web_extent().MatchesSecurityOrigin(origin) &&
        process_map_.Contains((*i)->id(), process_id) &&
        (*i)->permissions_data()->HasAPIPermission(permission)) {
      extensions->Insert(*i);
    }
  }
}

bool InfoMap::SecurityOriginHasAPIPermission(const GURL& origin,
                                             int process_id,
                                             APIPermission::ID permission)
    const {
  ExtensionSet extensions;
  GetExtensionsWithAPIPermissionForSecurityOrigin(
      origin, process_id, permission, &extensions);
  return !extensions.is_empty();
}

QuotaService* InfoMap::GetQuotaService() {
  CheckOnValidThread();
  if (!quota_service_)
    quota_service_.reset(new QuotaService());
  return quota_service_.get();
}

void InfoMap::SetSigninProcess(int process_id) {
  signin_process_id_ = process_id;
}

bool InfoMap::IsSigninProcess(int process_id) const {
  return process_id == signin_process_id_;
}

void InfoMap::SetNotificationsDisabled(
    const std::string& extension_id,
    bool notifications_disabled) {
  ExtraDataMap::iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    iter->second.notifications_disabled = notifications_disabled;
}

bool InfoMap::AreNotificationsDisabled(
    const std::string& extension_id) const {
  ExtraDataMap::const_iterator iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.notifications_disabled;
  return false;
}

void InfoMap::SetContentVerifier(ContentVerifier* verifier) {
  content_verifier_ = verifier;
}

InfoMap::~InfoMap() {
  if (quota_service_) {
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE, quota_service_.release());
  }
}

}  // namespace extensions
