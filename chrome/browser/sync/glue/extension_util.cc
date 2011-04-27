// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_util.h"

#include <sstream>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

namespace browser_sync {

bool IsExtensionValid(const Extension& extension) {
  // TODO(akalin): Figure out if we need to allow some other types.
  if (extension.location() != Extension::INTERNAL) {
    // We have a non-standard location.
    return false;
  }

  // Disallow extensions with non-gallery auto-update URLs for now.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (!extension.update_url().is_empty() &&
      (extension.update_url() != Extension::GalleryUpdateUrl(false)) &&
      (extension.update_url() != Extension::GalleryUpdateUrl(true))) {
    return false;
  }

  // Disallow extensions with native code plugins.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (!extension.plugins().empty()) {
    return false;
  }

  return true;
}

std::string ExtensionSpecificsToString(
    const sync_pb::ExtensionSpecifics& specifics) {
  std::stringstream ss;
  ss << "{ ";
  ss << "id: "                   << specifics.id()                << ", ";
  ss << "version: "              << specifics.version()           << ", ";
  ss << "update_url: "           << specifics.update_url()        << ", ";
  ss << "enabled: "              << specifics.enabled()           << ", ";
  ss << "incognito_enabled: "    << specifics.incognito_enabled() << ", ";
  ss << "name: "                 << specifics.name();
  ss << " }";
  return ss.str();
}

bool SpecificsToSyncData(
    const sync_pb::ExtensionSpecifics& specifics,
    ExtensionSyncData* sync_data) {
  if (!Extension::IdIsValid(specifics.id())) {
    return false;
  }

  scoped_ptr<Version> version(
      Version::GetVersionFromString(specifics.version()));
  if (!version.get()) {
    return false;
  }

  // The update URL must be either empty or valid.
  GURL update_url(specifics.update_url());
  if (!update_url.is_empty() && !update_url.is_valid()) {
    return false;
  }

  sync_data->id = specifics.id();
  sync_data->update_url = update_url;
  sync_data->version = *version;
  sync_data->enabled = specifics.enabled();
  sync_data->incognito_enabled = specifics.incognito_enabled();
  sync_data->name = specifics.name();
  return true;
}

void SyncDataToSpecifics(
    const ExtensionSyncData& sync_data,
    sync_pb::ExtensionSpecifics* specifics) {
  DCHECK(Extension::IdIsValid(sync_data.id));
  DCHECK(!sync_data.uninstalled);
  specifics->set_id(sync_data.id);
  specifics->set_update_url(sync_data.update_url.spec());
  specifics->set_version(sync_data.version.GetString());
  specifics->set_enabled(sync_data.enabled);
  specifics->set_incognito_enabled(sync_data.incognito_enabled);
  specifics->set_name(sync_data.name);
}

}  // namespace browser_sync
