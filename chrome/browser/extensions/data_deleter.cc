// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/data_deleter.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/api/storage/storage_frontend.h"
#endif

using base::WeakPtr;
using content::BrowserContext;
using content::BrowserThread;
using content::StoragePartition;

namespace extensions {

namespace {

// Helper function that deletes data of a given |storage_origin| in a given
// |partition|.
void DeleteOrigin(Profile* profile,
                  StoragePartition* partition,
                  const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
  DCHECK(partition);

  if (origin.SchemeIs(kExtensionScheme)) {
    // TODO(ajwong): Cookies are not properly isolated for
    // chrome-extension:// scheme.  (http://crbug.com/158386).
    //
    // However, no isolated apps actually can write to kExtensionScheme
    // origins. Thus, it is benign to delete from the
    // RequestContextForExtensions because there's nothing stored there. We
    // preserve this code path without checking for isolation because it's
    // simpler than special casing.  This code should go away once we merge
    // the various URLRequestContexts (http://crbug.com/159193).
    partition->ClearDataForOrigin(
        ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
        origin,
        profile->GetRequestContextForExtensions());
  } else {
    // We don't need to worry about the media request context because that
    // shares the same cookie store as the main request context.
    partition->ClearDataForOrigin(
        ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
        origin,
        partition->GetURLRequestContext());
  }
}

void OnNeedsToGarbageCollectIsolatedStorage(WeakPtr<ExtensionService> es) {
  if (!es)
    return;
  ExtensionPrefs::Get(es->profile())->SetNeedsStorageGarbageCollection(true);
}

} // namespace

// static
void DataDeleter::StartDeleting(Profile* profile, const Extension* extension) {
  DCHECK(profile);
  DCHECK(extension);

  if (AppIsolationInfo::HasIsolatedStorage(extension)) {
    BrowserContext::AsyncObliterateStoragePartition(
        profile,
        util::GetSiteForExtensionId(extension->id(), profile),
        base::Bind(
            &OnNeedsToGarbageCollectIsolatedStorage,
            ExtensionSystem::Get(profile)->extension_service()->AsWeakPtr()));
  } else {
    GURL launch_web_url_origin(
        AppLaunchInfo::GetLaunchWebURL(extension).GetOrigin());

    StoragePartition* partition = BrowserContext::GetStoragePartitionForSite(
        profile,
        Extension::GetBaseURLFromExtensionId(extension->id()));

    if (extension->is_hosted_app() &&
        !profile->GetExtensionSpecialStoragePolicy()->
            IsStorageProtected(launch_web_url_origin)) {
      DeleteOrigin(profile, partition, launch_web_url_origin);
    }
    DeleteOrigin(profile, partition, extension->url());
  }

#if defined(ENABLE_EXTENSIONS)
  // Begin removal of the settings for the current extension.
  // StorageFrontend may not exist in unit tests.
  StorageFrontend* frontend = StorageFrontend::Get(profile);
  if (frontend)
    frontend->DeleteStorageSoon(extension->id());
#endif  // defined(ENABLE_EXTENSIONS)
}

}  // namespace extensions
