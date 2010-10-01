// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_util.h"

#include <sstream>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/version.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "googleurl/src/gurl.h"

namespace browser_sync {

ExtensionType GetExtensionType(const Extension& extension) {
  if (extension.is_theme()) {
    return THEME;
  }

  if (extension.is_app()) {
    return APP;
  }

  if (extension.converted_from_user_script()) {
    if (extension.update_url().is_empty()) {
      return LOCAL_USER_SCRIPT;
    }
    return UPDATEABLE_USER_SCRIPT;
  }

  // Otherwise, we just have a regular extension.
  return EXTENSION;
}

// Keep this in sync with the above function.
// TODO(akalin): Or just hurry up and remove this!
ExtensionType GetExtensionTypeFromUninstalledExtensionInfo(
    const UninstalledExtensionInfo& uninstalled_extension_info) {
  if (uninstalled_extension_info.is_theme) {
    return THEME;
  }

  if (uninstalled_extension_info.is_app) {
    return APP;
  }

  if (uninstalled_extension_info.converted_from_user_script) {
    if (uninstalled_extension_info.update_url.is_empty()) {
      return LOCAL_USER_SCRIPT;
    }
    return UPDATEABLE_USER_SCRIPT;
  }
  return EXTENSION;
}

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
      (extension.update_url() !=
       GURL(extension_urls::kGalleryUpdateHttpUrl)) &&
      (extension.update_url() !=
       GURL(extension_urls::kGalleryUpdateHttpsUrl))) {
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

bool IsExtensionValidAndSyncable(const Extension& extension,
                                 const ExtensionTypeSet& allowed_types) {
  return
      IsExtensionValid(extension) &&
      ContainsKey(allowed_types, GetExtensionType(extension));
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

bool IsExtensionSpecificsValid(
    const sync_pb::ExtensionSpecifics& specifics) {
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

  return true;
}

void DcheckIsExtensionSpecificsValid(
    const sync_pb::ExtensionSpecifics& specifics) {
  DCHECK(IsExtensionSpecificsValid(specifics))
      << ExtensionSpecificsToString(specifics);
}

bool AreExtensionSpecificsEqual(const sync_pb::ExtensionSpecifics& a,
                                const sync_pb::ExtensionSpecifics& b) {
  // TODO(akalin): Figure out if we have to worry about version/URL
  // strings that are not identical but map to the same object.
  return ((a.id() == b.id()) &&
          (a.version() == b.version()) &&
          (a.update_url() == b.update_url()) &&
          (a.enabled() == b.enabled()) &&
          (a.incognito_enabled() == b.incognito_enabled()) &&
          (a.name() == b.name()));
}

bool IsExtensionSpecificsUnset(
    const sync_pb::ExtensionSpecifics& specifics) {
  return AreExtensionSpecificsEqual(specifics,
                                    sync_pb::ExtensionSpecifics());
}

void CopyUserProperties(
    const sync_pb::ExtensionSpecifics& specifics,
    sync_pb::ExtensionSpecifics* dest_specifics) {
  DCHECK(dest_specifics);
  dest_specifics->set_enabled(specifics.enabled());
  dest_specifics->set_incognito_enabled(specifics.incognito_enabled());
}

void CopyNonUserProperties(
    const sync_pb::ExtensionSpecifics& specifics,
    sync_pb::ExtensionSpecifics* dest_specifics) {
  DCHECK(dest_specifics);
  sync_pb::ExtensionSpecifics old_dest_specifics(*dest_specifics);
  *dest_specifics = specifics;
  CopyUserProperties(old_dest_specifics, dest_specifics);
}

bool AreExtensionSpecificsUserPropertiesEqual(
    const sync_pb::ExtensionSpecifics& a,
    const sync_pb::ExtensionSpecifics& b) {
  sync_pb::ExtensionSpecifics a_user_properties, b_user_properties;
  CopyUserProperties(a, &a_user_properties);
  CopyUserProperties(b, &b_user_properties);
  return AreExtensionSpecificsEqual(a_user_properties, b_user_properties);
}

bool AreExtensionSpecificsNonUserPropertiesEqual(
    const sync_pb::ExtensionSpecifics& a,
    const sync_pb::ExtensionSpecifics& b) {
  sync_pb::ExtensionSpecifics a_non_user_properties, b_non_user_properties;
  CopyNonUserProperties(a, &a_non_user_properties);
  CopyNonUserProperties(b, &b_non_user_properties);
  return AreExtensionSpecificsEqual(
      a_non_user_properties, b_non_user_properties);
}

void GetExtensionSpecifics(const Extension& extension,
                           ExtensionsService* extensions_service,
                           sync_pb::ExtensionSpecifics* specifics) {
  const std::string& id = extension.id();
  bool enabled =
      extensions_service->GetExtensionById(id, false) != NULL;
  bool incognito_enabled =
      extensions_service->IsIncognitoEnabled(&extension);
  GetExtensionSpecificsHelper(extension, enabled, incognito_enabled,
                              specifics);
}

void GetExtensionSpecificsHelper(const Extension& extension,
                                 bool enabled, bool incognito_enabled,
                                 sync_pb::ExtensionSpecifics* specifics) {
  DCHECK(IsExtensionValid(extension));
  const std::string& id = extension.id();
  specifics->set_id(id);
  specifics->set_version(extension.VersionString());
  specifics->set_update_url(extension.update_url().spec());
  specifics->set_enabled(enabled);
  specifics->set_incognito_enabled(incognito_enabled);
  specifics->set_name(extension.name());
  DcheckIsExtensionSpecificsValid(*specifics);
}

bool IsExtensionOutdated(const Extension& extension,
                         const sync_pb::ExtensionSpecifics& specifics) {
  DCHECK(IsExtensionValid(extension));
  DcheckIsExtensionSpecificsValid(specifics);
  scoped_ptr<Version> specifics_version(
      Version::GetVersionFromString(specifics.version()));
  if (!specifics_version.get()) {
    // If version is invalid, assume we're up-to-date.
    return false;
  }
  return extension.version()->CompareTo(*specifics_version) < 0;
}

void SetExtensionProperties(
    const sync_pb::ExtensionSpecifics& specifics,
    ExtensionsService* extensions_service, Extension* extension) {
  DcheckIsExtensionSpecificsValid(specifics);
  CHECK(extensions_service);
  CHECK(extension);
  DCHECK(IsExtensionValid(*extension));
  const std::string& id = extension->id();
  GURL update_url(specifics.update_url());
  if (update_url != extension->update_url()) {
    LOG(WARNING) << "specifics for extension " << id
                 << "has a different update URL than the extension: "
                 << update_url.spec() << " vs. " << extension->update_url();
  }
  bool enabled = extensions_service->GetExtensionById(id, false) != NULL;
  if (enabled && !specifics.enabled()) {
    extensions_service->DisableExtension(id);
  } else if (!enabled && specifics.enabled()) {
    extensions_service->EnableExtension(id);
  }
  bool incognito_enabled =
      extensions_service->IsIncognitoEnabled(extension);
  if (incognito_enabled != specifics.incognito_enabled()) {
    extensions_service->SetIsIncognitoEnabled(
        extension, specifics.incognito_enabled());
  }
  if (specifics.name() != extension->name()) {
    LOG(WARNING) << "specifics for extension " << id
                 << "has a different name than the extension: "
                 << specifics.name() << " vs. " << extension->name();
  }
}

void MergeExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& specifics,
    bool merge_user_properties,
    sync_pb::ExtensionSpecifics* merged_specifics) {
  DcheckIsExtensionSpecificsValid(*merged_specifics);
  DcheckIsExtensionSpecificsValid(specifics);
  DCHECK_EQ(specifics.id(), merged_specifics->id());
  // TODO(akalin): Merge enabled permissions when we sync those.
  scoped_ptr<Version> version(
      Version::GetVersionFromString(specifics.version()));
  CHECK(version.get());
  scoped_ptr<Version> merged_version(
      Version::GetVersionFromString(merged_specifics->version()));
  CHECK(merged_version.get());
  if (version->CompareTo(*merged_version) >= 0) {
    // |specifics| has a more recent or the same version, so merge it
    // in.
    CopyNonUserProperties(specifics, merged_specifics);
    if (merge_user_properties) {
      CopyUserProperties(specifics, merged_specifics);
    }
  }
}

}  // namespace browser_sync
