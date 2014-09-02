// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/sync_helper.h"

#include "base/logging.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace sync_helper {

namespace {

enum SyncType {
  SYNC_TYPE_NONE = 0,
  SYNC_TYPE_EXTENSION,
  SYNC_TYPE_APP
};

SyncType GetSyncType(const Extension* extension) {
  if (!IsSyncable(extension)) {
    // We have a non-standard location.
    return SYNC_TYPE_NONE;
  }

  // Disallow extensions with non-gallery auto-update URLs for now.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (!ManifestURL::GetUpdateURL(extension).is_empty() &&
      !ManifestURL::UpdatesFromGallery(extension)) {
    return SYNC_TYPE_NONE;
  }

  // Disallow extensions with native code plugins.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (PluginInfo::HasPlugins(extension) ||
      extension->permissions_data()->HasAPIPermission(APIPermission::kPlugin)) {
    return SYNC_TYPE_NONE;
  }

  switch (extension->GetType()) {
    case Manifest::TYPE_EXTENSION:
      return SYNC_TYPE_EXTENSION;

    case Manifest::TYPE_USER_SCRIPT:
      // We only want to sync user scripts with gallery update URLs.
      if (ManifestURL::UpdatesFromGallery(extension))
        return SYNC_TYPE_EXTENSION;
      return SYNC_TYPE_NONE;

    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
      return SYNC_TYPE_APP;

    case Manifest::TYPE_UNKNOWN:
    // Confusingly, themes are actually synced.
    // TODO(yoz): Make this look less inconsistent.
    case Manifest::TYPE_THEME:
    case Manifest::TYPE_SHARED_MODULE:
      return SYNC_TYPE_NONE;

    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return SYNC_TYPE_NONE;
}

}  // namespace

bool IsSyncable(const Extension* extension) {
  // TODO(akalin): Figure out if we need to allow some other types.

  // Default apps are not synced because otherwise they will pollute profiles
  // that don't already have them. Specially, if a user doesn't have default
  // apps, creates a new profile (which get default apps) and then enables sync
  // for it, then their profile everywhere gets the default apps.
  bool is_syncable = (extension->location() == Manifest::INTERNAL &&
                      !extension->was_installed_by_default());
  // Sync the chrome web store to maintain its position on the new tab page.
  is_syncable |= (extension->id() == extensions::kWebStoreAppId);
  // Sync the chrome component app to maintain its position on the app list.
  is_syncable |= (extension->id() == extension_misc::kChromeAppId);
  return is_syncable;
}

bool IsSyncableExtension(const Extension* extension) {
  return GetSyncType(extension) == SYNC_TYPE_EXTENSION;
}

bool IsSyncableApp(const Extension* extension) {
  return GetSyncType(extension) == SYNC_TYPE_APP;
}

}  // namespace sync_helper
}  // namespace extensions
