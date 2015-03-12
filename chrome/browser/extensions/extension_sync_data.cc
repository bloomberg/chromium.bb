// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/extension_service.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_url_handlers.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

namespace {

std::string GetExtensionSpecificsLogMessage(
    const sync_pb::ExtensionSpecifics& specifics) {
  return base::StringPrintf("id: %s\nversion: %s\nupdate_url: %s",
                            specifics.id().c_str(),
                            specifics.version().c_str(),
                            specifics.update_url().c_str());
}

enum BadSyncDataReason {
  // Invalid extension ID.
  BAD_EXTENSION_ID,

  // Invalid version.
  BAD_VERSION,

  // Invalid update URL.
  BAD_UPDATE_URL,

  // No ExtensionSpecifics in the EntitySpecifics.
  NO_EXTENSION_SPECIFICS,

  // Must be at the end.
  NUM_BAD_SYNC_DATA_REASONS
};

void RecordBadSyncData(BadSyncDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.BadSyncDataReason", reason,
                            NUM_BAD_SYNC_DATA_REASONS);
}

}  // namespace

ExtensionSyncData::ExtensionSyncData()
    : uninstalled_(false),
      enabled_(false),
      incognito_enabled_(false),
      remote_install_(false),
      all_urls_enabled_(BOOLEAN_UNSET),
      installed_by_custodian_(false) {
}

ExtensionSyncData::ExtensionSyncData(const Extension& extension,
                                     bool enabled,
                                     bool incognito_enabled,
                                     bool remote_install,
                                     OptionalBoolean all_urls_enabled)
    : id_(extension.id()),
      uninstalled_(false),
      enabled_(enabled),
      incognito_enabled_(incognito_enabled),
      remote_install_(remote_install),
      all_urls_enabled_(all_urls_enabled),
      installed_by_custodian_(extension.was_installed_by_custodian()),
      version_(extension.from_bookmark() ? base::Version("0")
                                         : *extension.version()),
      update_url_(ManifestURL::GetUpdateURL(&extension)),
      name_(extension.non_localized_name()) {
}

ExtensionSyncData::~ExtensionSyncData() {}

// static
scoped_ptr<ExtensionSyncData> ExtensionSyncData::CreateFromSyncData(
    const syncer::SyncData& sync_data) {
  scoped_ptr<ExtensionSyncData> data(new ExtensionSyncData);
  if (data->PopulateFromSyncData(sync_data))
    return data.Pass();
  return scoped_ptr<ExtensionSyncData>();
}

// static
scoped_ptr<ExtensionSyncData> ExtensionSyncData::CreateFromSyncChange(
    const syncer::SyncChange& sync_change) {
  scoped_ptr<ExtensionSyncData> data(
      CreateFromSyncData(sync_change.sync_data()));
  if (!data.get())
    return scoped_ptr<ExtensionSyncData>();

  data->set_uninstalled(sync_change.change_type() ==
                        syncer::SyncChange::ACTION_DELETE);
  return data.Pass();
}

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
  DCHECK(crx_file::id_util::IdIsValid(id_));
  specifics->set_id(id_);
  specifics->set_update_url(update_url_.spec());
  specifics->set_version(version_.GetString());
  specifics->set_enabled(enabled_);
  specifics->set_incognito_enabled(incognito_enabled_);
  specifics->set_remote_install(remote_install_);
  if (all_urls_enabled_ != BOOLEAN_UNSET)
    specifics->set_all_urls_enabled(all_urls_enabled_ == BOOLEAN_TRUE);
  specifics->set_installed_by_custodian(installed_by_custodian_);
  specifics->set_name(name_);
}

bool ExtensionSyncData::PopulateFromExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& specifics) {
  if (!crx_file::id_util::IdIsValid(specifics.id())) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad ID):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_EXTENSION_ID);
    return false;
  }

  Version specifics_version(specifics.version());
  if (!specifics_version.IsValid()) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad version):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_VERSION);
    return false;
  }

  // The update URL must be either empty or valid.
  GURL specifics_update_url(specifics.update_url());
  if (!specifics_update_url.is_empty() && !specifics_update_url.is_valid()) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad update URL):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_UPDATE_URL);
    return false;
  }

  id_ = specifics.id();
  update_url_ = specifics_update_url;
  version_ = specifics_version;
  enabled_ = specifics.enabled();
  incognito_enabled_ = specifics.incognito_enabled();
  if (specifics.has_all_urls_enabled()) {
    all_urls_enabled_ =
        specifics.all_urls_enabled() ? BOOLEAN_TRUE : BOOLEAN_FALSE;
  } else {
    // Set this explicitly (even though it's the default) on the offchance
    // that someone is re-using an ExtensionSyncData object.
    all_urls_enabled_ = BOOLEAN_UNSET;
  }
  remote_install_ = specifics.remote_install();
  installed_by_custodian_ = specifics.installed_by_custodian();
  name_ = specifics.name();
  return true;
}

void ExtensionSyncData::set_uninstalled(bool uninstalled) {
  uninstalled_ = uninstalled;
}

bool ExtensionSyncData::PopulateFromSyncData(
    const syncer::SyncData& sync_data) {
  const sync_pb::EntitySpecifics& entity_specifics = sync_data.GetSpecifics();

  if (entity_specifics.has_extension())
    return PopulateFromExtensionSpecifics(entity_specifics.extension());

  LOG(ERROR) << "Attempt to sync bad EntitySpecifics: no extension data.";
  RecordBadSyncData(NO_EXTENSION_SPECIFICS);
  return false;
}

}  // namespace extensions
