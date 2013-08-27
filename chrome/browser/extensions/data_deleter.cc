// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/data_deleter.h"

#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserContext;
using content::BrowserThread;
using content::StoragePartition;

namespace extensions {

// static
void DataDeleter::StartDeleting(Profile* profile,
                                const std::string& extension_id,
                                const GURL& storage_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  const GURL& site = Extension::GetBaseURLFromExtensionId(extension_id);

  BrowserContext::GetStoragePartitionForSite(profile, site)->
      ClearDataForOrigin((StoragePartition::REMOVE_DATA_MASK_ALL &
                          ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE),
                         StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                         storage_origin);

  // Begin removal of the settings for the current extension.
  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}

}  // namespace extensions
