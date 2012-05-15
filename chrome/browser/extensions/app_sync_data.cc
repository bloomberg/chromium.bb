// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_sync_data.h"

#include "chrome/common/extensions/extension.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

AppSyncData::AppSyncData() : notifications_disabled_(false) {}

AppSyncData::AppSyncData(const SyncData& sync_data)
    : notifications_disabled_(false) {
  PopulateFromSyncData(sync_data);
}

AppSyncData::AppSyncData(const SyncChange& sync_change)
    : notifications_disabled_(false) {
  PopulateFromSyncData(sync_change.sync_data());
  extension_sync_data_.set_uninstalled(
      sync_change.change_type() == SyncChange::ACTION_DELETE);
}

AppSyncData::AppSyncData(const Extension& extension,
                         bool enabled,
                         bool incognito_enabled,
                         const std::string& notifications_client_id,
                         bool notifications_disabled,
                         const StringOrdinal& app_launch_ordinal,
                         const StringOrdinal& page_ordinal)
    : extension_sync_data_(extension, enabled, incognito_enabled),
      notifications_client_id_(notifications_client_id),
      notifications_disabled_(notifications_disabled),
      app_launch_ordinal_(app_launch_ordinal),
      page_ordinal_(page_ordinal) {
}

AppSyncData::~AppSyncData() {}

SyncData AppSyncData::GetSyncData() const {
  sync_pb::EntitySpecifics specifics;
  PopulateAppSpecifics(specifics.mutable_app());

  return SyncData::CreateLocalData(extension_sync_data_.id(),
                                   extension_sync_data_.name(),
                                   specifics);
}

SyncChange AppSyncData::GetSyncChange(SyncChange::SyncChangeType change_type)
    const {
  return SyncChange(change_type, GetSyncData());
}

void AppSyncData::PopulateAppSpecifics(sync_pb::AppSpecifics* specifics) const {
  DCHECK(specifics);
  sync_pb::AppNotificationSettings* notification_settings =
      specifics->mutable_notification_settings();
  if (!notifications_client_id_.empty())
    notification_settings->set_oauth_client_id(notifications_client_id_);
  notification_settings->set_disabled(notifications_disabled_);

  // Only sync the ordinal values if they are valid.
  if (app_launch_ordinal_.IsValid())
    specifics->set_app_launch_ordinal(app_launch_ordinal_.ToString());
  if (page_ordinal_.IsValid())
    specifics->set_page_ordinal(page_ordinal_.ToString());

  extension_sync_data_.PopulateExtensionSpecifics(
      specifics->mutable_extension());
}

void AppSyncData::PopulateFromAppSpecifics(
    const sync_pb::AppSpecifics& specifics) {
  extension_sync_data_.PopulateFromExtensionSpecifics(specifics.extension());

  if (specifics.has_notification_settings() &&
      specifics.notification_settings().has_oauth_client_id()) {
    notifications_client_id_ =
        specifics.notification_settings().oauth_client_id();
  }

  notifications_disabled_ =
      specifics.has_notification_settings() &&
      specifics.notification_settings().has_disabled() &&
      specifics.notification_settings().disabled();

  app_launch_ordinal_ = StringOrdinal(specifics.app_launch_ordinal());
  page_ordinal_ = StringOrdinal(specifics.page_ordinal());
}

void AppSyncData::PopulateFromSyncData(const SyncData& sync_data) {
  PopulateFromAppSpecifics(sync_data.GetSpecifics().app());
}

}  // namespace extensions
