// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_special_storage_policy.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "storage/common/quota/quota_types.h"

using content::BrowserThread;
using extensions::APIPermission;
using extensions::Extension;
using storage::SpecialStoragePolicy;

namespace {

void ReportQuotaUsage(storage::QuotaStatusCode code,
                      int64_t usage,
                      int64_t quota) {
  if (code == storage::kQuotaStatusOk) {
    // We're interested in the amount of space hosted apps are using. Record it
    // when the extension is granted the unlimited storage permission (once per
    // extension load, so on average once per run).
    UMA_HISTOGRAM_MEMORY_KB("Extensions.HostedAppUnlimitedStorageUsage", usage);
  }
}

// Log the usage for a hosted app with unlimited storage.
void LogHostedAppUnlimitedStorageUsage(
    scoped_refptr<const Extension> extension,
    content::BrowserContext* browser_context) {
  GURL launch_url =
      extensions::AppLaunchInfo::GetLaunchWebURL(extension.get()).GetOrigin();
  content::StoragePartition* partition =
      browser_context ?  // |browser_context| can be NULL in unittests.
      content::BrowserContext::GetStoragePartitionForSite(browser_context,
                                                          launch_url) :
      NULL;
  if (partition) {
    // We only have to query for kStorageTypePersistent data usage, because apps
    // cannot ask for any more temporary storage, according to
    // https://developers.google.com/chrome/whitepapers/storage.
    BrowserThread::PostAfterStartupTask(
        FROM_HERE, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
        base::BindOnce(&storage::QuotaManager::GetUsageAndQuotaForWebApps,
                       partition->GetQuotaManager(), launch_url,
                       storage::kStorageTypePersistent,
                       base::Bind(&ReportQuotaUsage)));
  }
}

}  // namespace

ExtensionSpecialStoragePolicy::ExtensionSpecialStoragePolicy(
    content_settings::CookieSettings* cookie_settings)
    : cookie_settings_(cookie_settings) {
}

ExtensionSpecialStoragePolicy::~ExtensionSpecialStoragePolicy() {}

bool ExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  if (origin.SchemeIs(extensions::kExtensionScheme))
    return true;
  base::AutoLock locker(lock_);
  return protected_apps_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnlimitedStorage))
    return true;

  if (origin.SchemeIs(content::kChromeDevToolsScheme) &&
      origin.host_piece() == chrome::kChromeUIDevToolsHost)
    return true;

  base::AutoLock locker(lock_);
  return unlimited_extensions_.Contains(origin) ||
         content_capabilities_unlimited_extensions_.GrantsCapabilitiesTo(
             origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  if (cookie_settings_.get() == NULL)
    return false;
  return cookie_settings_->IsCookieSessionOnly(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageSessionOnlyOrBlocked(
    const GURL& origin) {
  if (cookie_settings_.get() == NULL)
    return false;
  return cookie_settings_->IsCookieSessionOnlyOrBlocked(origin);
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
    if (entries[i].GetContentSetting() == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

bool ExtensionSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  base::AutoLock locker(lock_);
  return isolated_extensions_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageDurable(const GURL& origin) {
  return cookie_settings_->IsStorageDurable(origin);
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
    const extensions::Extension* extension,
    content::BrowserContext* browser_context) {
  base::AutoLock locker(lock_);
  DCHECK(extension);

  int change_flags = 0;
  if (extensions::ContentCapabilitiesInfo::Get(extension)
          .permissions.count(APIPermission::kUnlimitedStorage) > 0) {
    content_capabilities_unlimited_extensions_.Add(extension);
    change_flags |= SpecialStoragePolicy::STORAGE_UNLIMITED;
  }

  if (NeedsProtection(extension) ||
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kUnlimitedStorage) ||
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kFileBrowserHandler) ||
      extensions::AppIsolationInfo::HasIsolatedStorage(extension) ||
      extension->is_app()) {
    if (NeedsProtection(extension) && protected_apps_.Add(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_PROTECTED;

    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kUnlimitedStorage) &&
        unlimited_extensions_.Add(extension)) {
      if (extension->is_hosted_app())
        LogHostedAppUnlimitedStorageUsage(extension, browser_context);
      change_flags |= SpecialStoragePolicy::STORAGE_UNLIMITED;
    }

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
  base::AutoLock locker(lock_);
  DCHECK(extension);

  int change_flags = 0;
  if (extensions::ContentCapabilitiesInfo::Get(extension)
          .permissions.count(APIPermission::kUnlimitedStorage) > 0) {
    content_capabilities_unlimited_extensions_.Remove(extension);
    change_flags |= SpecialStoragePolicy::STORAGE_UNLIMITED;
  }

  if (NeedsProtection(extension) ||
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kUnlimitedStorage) ||
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kFileBrowserHandler) ||
      extensions::AppIsolationInfo::HasIsolatedStorage(extension) ||
      extension->is_app()) {
    if (NeedsProtection(extension) && protected_apps_.Remove(extension))
      change_flags |= SpecialStoragePolicy::STORAGE_PROTECTED;

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
    unlimited_extensions_.Clear();
    file_handler_extensions_.Clear();
    isolated_extensions_.Clear();
    content_capabilities_unlimited_extensions_.Clear();
  }

  NotifyCleared();
}

void ExtensionSpecialStoragePolicy::NotifyGranted(
    const GURL& origin,
    int change_flags) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ExtensionSpecialStoragePolicy::NotifyGranted, this,
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
        base::BindOnce(&ExtensionSpecialStoragePolicy::NotifyRevoked, this,
                       origin, change_flags));
    return;
  }
  SpecialStoragePolicy::NotifyRevoked(origin, change_flags);
}

void ExtensionSpecialStoragePolicy::NotifyCleared() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ExtensionSpecialStoragePolicy::NotifyCleared, this));
    return;
  }
  SpecialStoragePolicy::NotifyCleared();
}

//-----------------------------------------------------------------------------
// SpecialCollection helper class
//-----------------------------------------------------------------------------

ExtensionSpecialStoragePolicy::SpecialCollection::SpecialCollection() {}

ExtensionSpecialStoragePolicy::SpecialCollection::~SpecialCollection() {}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Contains(
    const GURL& origin) {
  return !ExtensionsContaining(origin)->is_empty();
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::GrantsCapabilitiesTo(
    const GURL& origin) {
  for (const auto& extension : extensions_) {
    if (extensions::ContentCapabilitiesInfo::Get(extension.get())
            .url_patterns.MatchesURL(origin)) {
      return true;
    }
  }
  return false;
}

const extensions::ExtensionSet*
ExtensionSpecialStoragePolicy::SpecialCollection::ExtensionsContaining(
    const GURL& origin) {
  std::unique_ptr<extensions::ExtensionSet>& result = cached_results_[origin];
  if (result)
    return result.get();

  result = base::MakeUnique<extensions::ExtensionSet>();
  for (auto& extension : extensions_) {
    if (extension->OverlapsWithOrigin(origin))
      result->Insert(extension);
  }

  return result.get();
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
  cached_results_.clear();
}
