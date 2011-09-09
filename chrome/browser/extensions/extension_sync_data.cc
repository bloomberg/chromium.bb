// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"

ExtensionSyncData::ExtensionSyncData()
    : uninstalled_(false),
      enabled_(false),
      incognito_enabled_(false),
      type_(Extension::SYNC_TYPE_NONE) {
}

ExtensionSyncData::ExtensionSyncData(const SyncData& sync_data)
    : uninstalled_(false),
      enabled_(false),
      incognito_enabled_(false),
      type_(Extension::SYNC_TYPE_NONE) {
  PopulateFromSyncData(sync_data);
}

ExtensionSyncData::ExtensionSyncData(const SyncChange& sync_change)
    : uninstalled_(sync_change.change_type() == SyncChange::ACTION_DELETE) {
  PopulateFromSyncData(sync_change.sync_data());
}

ExtensionSyncData::ExtensionSyncData(const Extension& extension,
                                     bool enabled,
                                     bool incognito_enabled)
  : id_(extension.id()),
    uninstalled_(false),
    enabled_(enabled),
    incognito_enabled_(incognito_enabled),
    type_(extension.GetSyncType()),
    version_(*extension.version()),
    update_url_(extension.update_url()),
    name_(extension.name()) {
}

ExtensionSyncData::~ExtensionSyncData() {}

void ExtensionSyncData::PopulateSyncSpecifics(
    sync_pb::ExtensionSpecifics* specifics) const {
  DCHECK(Extension::IdIsValid(id_));
  specifics->set_id(id_);
  specifics->set_update_url(update_url_.spec());
  specifics->set_version(version_.GetString());
  specifics->set_enabled(enabled_);
  specifics->set_incognito_enabled(incognito_enabled_);
  specifics->set_name(name_);
}

SyncData ExtensionSyncData::GetSyncData() const {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* extension_specifics = NULL;

  switch (type_) {
    case Extension::SYNC_TYPE_EXTENSION:
      extension_specifics = specifics.MutableExtension(sync_pb::extension);
      break;
    case Extension::SYNC_TYPE_APP:
      extension_specifics =
          specifics.MutableExtension(sync_pb::app)->mutable_extension();
      break;
    default:
      LOG(FATAL) << "Attempt to get non-syncable data.";
  }

  PopulateSyncSpecifics(extension_specifics);

  return SyncData::CreateLocalData(id_, name_, specifics);
}

SyncChange ExtensionSyncData::GetSyncChange(
    SyncChange::SyncChangeType change_type) const {
  return SyncChange(change_type, GetSyncData());
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
  name_ = specifics.name();
}

void ExtensionSyncData::PopulateFromSyncData(const SyncData& sync_data) {
  const sync_pb::EntitySpecifics& entity_specifics = sync_data.GetSpecifics();
  sync_pb::ExtensionSpecifics extension_expecifics;
  if (entity_specifics.HasExtension(sync_pb::extension)) {
    extension_expecifics = entity_specifics.GetExtension(sync_pb::extension);
    type_ = Extension::SYNC_TYPE_EXTENSION;
  } else if (entity_specifics.HasExtension(sync_pb::app)) {
    extension_expecifics =
        entity_specifics.GetExtension(sync_pb::app).extension();
    type_ = Extension::SYNC_TYPE_APP;
  } else {
    LOG(FATAL) << "Attempt to sync bad EntitySpecifics.";
  }
  PopulateFromExtensionSpecifics(extension_expecifics);
}

