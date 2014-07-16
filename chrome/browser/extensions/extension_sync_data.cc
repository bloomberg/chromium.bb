// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/logging.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/common/extension.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

ExtensionSyncData::ExtensionSyncData()
    : uninstalled_(false),
      enabled_(false),
      incognito_enabled_(false),
      remote_install_(false),
      installed_by_custodian_(false) {
}

ExtensionSyncData::ExtensionSyncData(const syncer::SyncData& sync_data)
    : uninstalled_(false),
      enabled_(false),
      incognito_enabled_(false),
      remote_install_(false),
      installed_by_custodian_(false) {
  PopulateFromSyncData(sync_data);
}

ExtensionSyncData::ExtensionSyncData(const syncer::SyncChange& sync_change)
    : uninstalled_(sync_change.change_type() ==
                   syncer::SyncChange::ACTION_DELETE),
      enabled_(false),
      incognito_enabled_(false),
      remote_install_(false),
      installed_by_custodian_(false) {
  PopulateFromSyncData(sync_change.sync_data());
}

ExtensionSyncData::ExtensionSyncData(const Extension& extension,
                                     bool enabled,
                                     bool incognito_enabled,
                                     bool remote_install)
    : id_(extension.id()),
      uninstalled_(false),
      enabled_(enabled),
      incognito_enabled_(incognito_enabled),
      remote_install_(remote_install),
      installed_by_custodian_(extension.was_installed_by_custodian()),
      version_(extension.from_bookmark() ? base::Version("0")
                                         : *extension.version()),
      update_url_(ManifestURL::GetUpdateURL(&extension)),
      name_(extension.non_localized_name()) {
}

ExtensionSyncData::~ExtensionSyncData() {}

syncer::SyncData ExtensionSyncData::GetSyncData() const {
  sync_pb::EntitySpecifics specifics;
  PopulateExtensionSpecifics(specifics.mutable_extension());

  return syncer::SyncData::CreateLocalData(id_, name_, specifics);
}

syncer::SyncChange ExtensionSyncData::GetSyncChange(
    syncer::SyncChange::SyncChangeType change_type) const {
  return syncer::SyncChange(FROM_HERE, change_type, GetSyncData());
}

void ExtensionSyncData::PopulateExtensionSpecifics(
    sync_pb::ExtensionSpecifics* specifics) const {
  DCHECK(Extension::IdIsValid(id_));
  specifics->set_id(id_);
  specifics->set_update_url(update_url_.spec());
  specifics->set_version(version_.GetString());
  specifics->set_enabled(enabled_);
  specifics->set_incognito_enabled(incognito_enabled_);
  specifics->set_remote_install(remote_install_);
  specifics->set_installed_by_custodian(installed_by_custodian_);
  specifics->set_name(name_);
}

void ExtensionSyncData::PopulateFromExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& specifics) {
  if (!Extension::IdIsValid(specifics.id())) {
    LOG(FATAL) << "Attempt to sync bad ExtensionSpecifics.";
  }

  Version specifics_version(specifics.version());
  if (!specifics_version.IsValid())
    LOG(FATAL) << "Attempt to sync bad ExtensionSpecifics.";

  // The update URL must be either empty or valid.
  GURL specifics_update_url(specifics.update_url());
  if (!specifics_update_url.is_empty() && !specifics_update_url.is_valid()) {
    LOG(FATAL) << "Attempt to sync bad ExtensionSpecifics.";
  }

  id_ = specifics.id();
  update_url_ = specifics_update_url;
  version_ = specifics_version;
  enabled_ = specifics.enabled();
  incognito_enabled_ = specifics.incognito_enabled();
  remote_install_ = specifics.remote_install();
  installed_by_custodian_ = specifics.installed_by_custodian();
  name_ = specifics.name();
}

void ExtensionSyncData::set_uninstalled(bool uninstalled) {
  uninstalled_ = uninstalled;
}

void ExtensionSyncData::PopulateFromSyncData(
    const syncer::SyncData& sync_data) {
  const sync_pb::EntitySpecifics& entity_specifics = sync_data.GetSpecifics();

  if (entity_specifics.has_extension()) {
    PopulateFromExtensionSpecifics(entity_specifics.extension());
  } else {
    LOG(FATAL) << "Attempt to sync bad EntitySpecifics.";
  }
}

}  // namespace extensions
