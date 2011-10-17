// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_special_storage_policy.h"

#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"

ExtensionSpecialStoragePolicy::ExtensionSpecialStoragePolicy(
    HostContentSettingsMap* host_content_settings_map)
    : host_content_settings_map_(host_content_settings_map) {}

ExtensionSpecialStoragePolicy::~ExtensionSpecialStoragePolicy() {}

bool ExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  if (origin.SchemeIs(chrome::kExtensionScheme))
    return true;
  base::AutoLock locker(lock_);
  return protected_apps_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  base::AutoLock locker(lock_);
  return unlimited_extensions_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  if (host_content_settings_map_ == NULL)
    return false;
  ContentSetting content_setting = host_content_settings_map_->
      GetCookieContentSetting(origin, origin, true);
  return (content_setting == CONTENT_SETTING_SESSION_ONLY);
}

bool ExtensionSpecialStoragePolicy::HasSessionOnlyOrigins() {
  if (host_content_settings_map_ == NULL)
    return false;
  if (host_content_settings_map_->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_COOKIES) == CONTENT_SETTING_SESSION_ONLY)
    return true;
  HostContentSettingsMap::SettingsForOneType entries;
  host_content_settings_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].c == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

bool ExtensionSpecialStoragePolicy::IsFileHandler(
    const std::string& extension_id) {
  base::AutoLock locker(lock_);
  return file_handler_extensions_.ContainsExtension(extension_id);
}

void ExtensionSpecialStoragePolicy::GrantRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasAPIPermission(
          ExtensionAPIPermission::kUnlimitedStorage) &&
      !extension->HasAPIPermission(
          ExtensionAPIPermission::kFileBrowserHandler)) {
    return;
  }
  {
    base::AutoLock locker(lock_);
    if (extension->is_hosted_app())
      protected_apps_.Add(extension);
    if (extension->HasAPIPermission(ExtensionAPIPermission::kUnlimitedStorage))
      unlimited_extensions_.Add(extension);
    if (extension->HasAPIPermission(
            ExtensionAPIPermission::kFileBrowserHandler)) {
      file_handler_extensions_.Add(extension);
    }
  }
  NotifyChanged();
}

void ExtensionSpecialStoragePolicy::RevokeRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasAPIPermission(
          ExtensionAPIPermission::kUnlimitedStorage) &&
      !extension->HasAPIPermission(
          ExtensionAPIPermission::kFileBrowserHandler)) {
    return;
  }
  {
    base::AutoLock locker(lock_);
    if (extension->is_hosted_app())
      protected_apps_.Remove(extension);
    if (extension->HasAPIPermission(ExtensionAPIPermission::kUnlimitedStorage))
      unlimited_extensions_.Remove(extension);
    if (extension->HasAPIPermission(
            ExtensionAPIPermission::kFileBrowserHandler)) {
      file_handler_extensions_.Remove(extension);
    }
  }
  NotifyChanged();
}

void ExtensionSpecialStoragePolicy::RevokeRightsForAllExtensions() {
  {
    base::AutoLock locker(lock_);
    protected_apps_.Clear();
    unlimited_extensions_.Clear();
    file_handler_extensions_.Clear();
  }
  NotifyChanged();
}

void ExtensionSpecialStoragePolicy::NotifyChanged() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &ExtensionSpecialStoragePolicy::NotifyChanged));
    return;
  }
  SpecialStoragePolicy::NotifyObservers();
}

//-----------------------------------------------------------------------------
// SpecialCollection helper class
//-----------------------------------------------------------------------------

ExtensionSpecialStoragePolicy::SpecialCollection::SpecialCollection() {}

ExtensionSpecialStoragePolicy::SpecialCollection::~SpecialCollection() {}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Contains(
    const GURL& origin) {
  CachedResults::const_iterator found = cached_results_.find(origin);
  if (found != cached_results_.end())
    return found->second;

  for (Extensions::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (iter->second->OverlapsWithOrigin(origin)) {
      cached_results_[origin] = true;
      return true;
    }
  }
  cached_results_[origin] = false;
  return false;
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::ContainsExtension(
    const std::string& extension_id) {
  return extensions_.find(extension_id) != extensions_.end();
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Add(
    const Extension* extension) {
  cached_results_.clear();
  extensions_[extension->id()] = extension;
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Remove(
    const Extension* extension) {
  cached_results_.clear();
  extensions_.erase(extension->id());
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Clear() {
  cached_results_.clear();
  extensions_.clear();
}
