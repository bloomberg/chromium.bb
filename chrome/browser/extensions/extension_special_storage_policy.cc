// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_special_storage_policy.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;
using extensions::APIPermission;
using extensions::Extension;
using quota::SpecialStoragePolicy;

ExtensionSpecialStoragePolicy::ExtensionSpecialStoragePolicy(
    CookieSettings* cookie_settings)
    : cookie_settings_(cookie_settings) {}

ExtensionSpecialStoragePolicy::~ExtensionSpecialStoragePolicy() {}

bool ExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  if (origin.SchemeIs(extensions::kExtensionScheme))
    return true;
  base::AutoLock locker(lock_);
  return protected_apps_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUnlimitedStorage))
    return true;

  if (origin.SchemeIs(content::kChromeDevToolsScheme) &&
      origin.host() == chrome::kChromeUIDevToolsHost)
    return true;

  base::AutoLock locker(lock_);
  return unlimited_extensions_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  if (cookie_settings_.get() == NULL)
    return false;
  return cookie_settings_->IsCookieSessionOnly(origin);
}

bool ExtensionSpecialStoragePolicy::CanQueryDiskSize(const GURL& origin) {
  return installed_apps_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::HasSessionOnlyOrigins() {
  if (cookie_settings_.get() == NULL)
    return false;
  if (cookie_settings_->GetDefaultCookieSetting(NULL) ==
      CONTENT_SETTING_SESSION_ONLY)
    return true;
  ContentSettingsForOneType entries;
  cookie_settings_->GetCookieSettings(&entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].setting == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

bool ExtensionSpecialStoragePolicy::IsFileHandler(
    const std::string& extension_id) {
  base::AutoLock locker(lock_);
  return file_handler_extensions_.ContainsExtension(extension_id);
}

bool ExtensionSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  base::AutoLock locker(lock_);
  return isolated_extensions_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::NeedsProtection(
    const extensions::Extension* extension) {
  return extension->is_hosted_app() && !extension->from_bookmark();
}

const extensions::ExtensionSet*
ExtensionSpecialStoragePolicy::ExtensionsProtectingOrigin(
    const GURL& origin) {
  base::AutoLock locker(lock_);
  return protected_apps_.ExtensionsContaining(origin);
}

void ExtensionSpecialStoragePolicy::GrantRightsForExtension(
    const extensions::Extension* extension) {
  DCHECK(extension);
  if (!(NeedsProtection(extension) ||
        extension->permissions_data()->HasAPIPermission(
            APIPermission::kUnlimitedStorage) ||
        extension->permissions_data()->HasAPIPermission(
            APIPermission::kFileBrowserHandler) ||
        extensions::AppIsolationInfo::HasIsolatedStorage(extension) ||
        extension->is_app())) {
    return;
  }

  int change_flags = 0;
  {
    base::AutoLock locker(lock_);
    if (NeedsProtection(extension) && protected_apps_.Add(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_PROTECTED;
    // FIXME: Does GrantRightsForExtension imply |extension| is installed?
    if (extension->is_app())
      installed_apps_.Add(extension);

    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kUnlimitedStorage) &&
        unlimited_extensions_.Add(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_UNLIMITED;

    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kFileBrowserHandler))
      file_handler_extensions_.Add(extension);

    if (extensions::AppIsolationInfo::HasIsolatedStorage(extension))
      isolated_extensions_.Add(extension);
  }

  if (change_flags) {
    NotifyGranted(Extension::GetBaseURLFromExtensionId(extension->id()),
                  change_flags);
  }
}

void ExtensionSpecialStoragePolicy::RevokeRightsForExtension(
    const extensions::Extension* extension) {
  DCHECK(extension);
  if (!(NeedsProtection(extension) ||
        extension->permissions_data()->HasAPIPermission(
            APIPermission::kUnlimitedStorage) ||
        extension->permissions_data()->HasAPIPermission(
            APIPermission::kFileBrowserHandler) ||
        extensions::AppIsolationInfo::HasIsolatedStorage(extension) ||
        extension->is_app())) {
    return;
  }
  int change_flags = 0;
  {
    base::AutoLock locker(lock_);
    if (NeedsProtection(extension) && protected_apps_.Remove(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_PROTECTED;

    if (extension->is_app())
      installed_apps_.Remove(extension);

    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kUnlimitedStorage) &&
        unlimited_extensions_.Remove(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_UNLIMITED;

    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kFileBrowserHandler))
      file_handler_extensions_.Remove(extension);

    if (extensions::AppIsolationInfo::HasIsolatedStorage(extension))
      isolated_extensions_.Remove(extension);
  }

  if (change_flags) {
    NotifyRevoked(Extension::GetBaseURLFromExtensionId(extension->id()),
                  change_flags);
  }
}

void ExtensionSpecialStoragePolicy::RevokeRightsForAllExtensions() {
  {
    base::AutoLock locker(lock_);
    protected_apps_.Clear();
    installed_apps_.Clear();
    unlimited_extensions_.Clear();
    file_handler_extensions_.Clear();
    isolated_extensions_.Clear();
  }

  NotifyCleared();
}

void ExtensionSpecialStoragePolicy::NotifyGranted(
    const GURL& origin,
    int change_flags) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ExtensionSpecialStoragePolicy::NotifyGranted, this,
                   origin, change_flags));
    return;
  }
  SpecialStoragePolicy::NotifyGranted(origin, change_flags);
}

void ExtensionSpecialStoragePolicy::NotifyRevoked(
    const GURL& origin,
    int change_flags) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ExtensionSpecialStoragePolicy::NotifyRevoked, this,
                   origin, change_flags));
    return;
  }
  SpecialStoragePolicy::NotifyRevoked(origin, change_flags);
}

void ExtensionSpecialStoragePolicy::NotifyCleared() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ExtensionSpecialStoragePolicy::NotifyCleared, this));
    return;
  }
  SpecialStoragePolicy::NotifyCleared();
}

//-----------------------------------------------------------------------------
// SpecialCollection helper class
//-----------------------------------------------------------------------------

ExtensionSpecialStoragePolicy::SpecialCollection::SpecialCollection() {}

ExtensionSpecialStoragePolicy::SpecialCollection::~SpecialCollection() {
  STLDeleteValues(&cached_results_);
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Contains(
    const GURL& origin) {
  return !ExtensionsContaining(origin)->is_empty();
}

const extensions::ExtensionSet*
ExtensionSpecialStoragePolicy::SpecialCollection::ExtensionsContaining(
    const GURL& origin) {
  CachedResults::const_iterator found = cached_results_.find(origin);
  if (found != cached_results_.end())
    return found->second;

  extensions::ExtensionSet* result = new extensions::ExtensionSet();
  for (extensions::ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if ((*iter)->OverlapsWithOrigin(origin))
      result->Insert(*iter);
  }
  cached_results_[origin] = result;
  return result;
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::ContainsExtension(
    const std::string& extension_id) {
  return extensions_.Contains(extension_id);
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Add(
    const extensions::Extension* extension) {
  ClearCache();
  return extensions_.Insert(extension);
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Remove(
    const extensions::Extension* extension) {
  ClearCache();
  return extensions_.Remove(extension->id());
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Clear() {
  ClearCache();
  extensions_.Clear();
}

void ExtensionSpecialStoragePolicy::SpecialCollection::ClearCache() {
  STLDeleteValues(&cached_results_);
  cached_results_.clear();
}
