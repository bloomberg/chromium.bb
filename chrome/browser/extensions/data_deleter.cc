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

namespace extensions {

// static
void DataDeleter::StartDeleting(Profile* profile,
                                const std::string& extension_id,
                                const GURL& storage_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  const GURL& site = Extension::GetBaseURLFromExtensionId(extension_id);

  content::StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(profile, site);

  if (storage_origin.SchemeIs(extensions::kExtensionScheme)) {
    // TODO(ajwong): Cookies are not properly isolated for
    // chrome-extension:// scheme.  (http://crbug.com/158386).
    //
    // However, no isolated apps actually can write to kExtensionScheme
    // origins. Thus, it is benign to delete from the
    // RequestContextForExtensions because there's nothing stored there. We
    // preserve this code path without checking for isolation because it's
    // simpler than special casing.  This code should go away once we merge
    // the various URLRequestContexts (http://crbug.com/159193).
    partition->AsyncClearDataForOrigin(
        content::StoragePartition::kAllStorage,
        storage_origin,
        profile->GetRequestContextForExtensions());
  } else {
    // We don't need to worry about the media request context because that
    // shares the same cookie store as the main request context.
    partition->AsyncClearDataForOrigin(content::StoragePartition::kAllStorage,
                                       storage_origin,
                                       partition->GetURLRequestContext());
  }

  // Begin removal of the settings for the current extension.
  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}

}  // namespace extensions
