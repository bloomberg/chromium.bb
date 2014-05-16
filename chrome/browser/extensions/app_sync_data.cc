// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_sync_data.h"

#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/common/extension.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

AppSyncData::AppSyncData() {}

AppSyncData::AppSyncData(const syncer::SyncData& sync_data) {
  PopulateFromSyncData(sync_data);
}

AppSyncData::AppSyncData(const syncer::SyncChange& sync_change) {
  PopulateFromSyncData(sync_change.sync_data());
  extension_sync_data_.set_uninstalled(
      sync_change.change_type() == syncer::SyncChange::ACTION_DELETE);
}

AppSyncData::AppSyncData(const Extension& extension,
                         bool enabled,
                         bool incognito_enabled,
                         bool remote_install,
                         const syncer::StringOrdinal& app_launch_ordinal,
                         const syncer::StringOrdinal& page_ordinal,
                         extensions::LaunchType launch_type)
    : extension_sync_data_(extension,
                           enabled,
                           incognito_enabled,
                           remote_install),
      app_launch_ordinal_(app_launch_ordinal),
      page_ordinal_(page_ordinal),
      launch_type_(launch_type) {
  if (extension.from_bookmark()) {
    bookmark_app_description_ = extension.description();
    bookmark_app_url_ = AppLaunchInfo::GetLaunchWebURL(&extension).spec();
  }
}

AppSyncData::~AppSyncData() {}

syncer::SyncData AppSyncData::GetSyncData() const {
  sync_pb::EntitySpecifics specifics;
  PopulateAppSpecifics(specifics.mutable_app());

  return syncer::SyncData::CreateLocalData(extension_sync_data_.id(),
                                   extension_sync_data_.name(),
                                   specifics);
}

syncer::SyncChange AppSyncData::GetSyncChange(
    syncer::SyncChange::SyncChangeType change_type) const {
  return syncer::SyncChange(FROM_HERE, change_type, GetSyncData());
}

void AppSyncData::PopulateAppSpecifics(sync_pb::AppSpecifics* specifics) const {
  DCHECK(specifics);
  // Only sync the ordinal values and launch type if they are valid.
  if (app_launch_ordinal_.IsValid())
    specifics->set_app_launch_ordinal(app_launch_ordinal_.ToInternalValue());
  if (page_ordinal_.IsValid())
    specifics->set_page_ordinal(page_ordinal_.ToInternalValue());

  sync_pb::AppSpecifics::LaunchType sync_launch_type =
      static_cast<sync_pb::AppSpecifics::LaunchType>(launch_type_);

  // The corresponding validation of this value during processing of an
  // AppSyncData is in ExtensionSyncService::ProcessAppSyncData.
  if (launch_type_ >= LAUNCH_TYPE_FIRST && launch_type_ < NUM_LAUNCH_TYPES &&
      sync_pb::AppSpecifics_LaunchType_IsValid(sync_launch_type)) {
    specifics->set_launch_type(sync_launch_type);
  }

  if (!bookmark_app_url_.empty())
    specifics->set_bookmark_app_url(bookmark_app_url_);

  if (!bookmark_app_description_.empty())
    specifics->set_bookmark_app_description(bookmark_app_description_);

  extension_sync_data_.PopulateExtensionSpecifics(
      specifics->mutable_extension());
}

void AppSyncData::PopulateFromAppSpecifics(
    const sync_pb::AppSpecifics& specifics) {
  extension_sync_data_.PopulateFromExtensionSpecifics(specifics.extension());

  app_launch_ordinal_ = syncer::StringOrdinal(specifics.app_launch_ordinal());
  page_ordinal_ = syncer::StringOrdinal(specifics.page_ordinal());

  launch_type_ = specifics.has_launch_type()
      ? static_cast<extensions::LaunchType>(specifics.launch_type())
      : LAUNCH_TYPE_INVALID;

  bookmark_app_url_ = specifics.bookmark_app_url();
  bookmark_app_description_ = specifics.bookmark_app_description();
}

void AppSyncData::PopulateFromSyncData(const syncer::SyncData& sync_data) {
  PopulateFromAppSpecifics(sync_data.GetSpecifics().app());
}

}  // namespace extensions
