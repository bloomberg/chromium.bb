// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_sync_data.h"

#include "chrome/common/extensions/manifest_handlers/app_icon_color_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"
#include "extensions/common/extension.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

AppSyncData::LinkedAppIconInfo::LinkedAppIconInfo() {
}

AppSyncData::LinkedAppIconInfo::~LinkedAppIconInfo() {
}

AppSyncData::AppSyncData() {}

AppSyncData::AppSyncData(const Extension& extension,
                         bool enabled,
                         int disable_reasons,
                         bool incognito_enabled,
                         bool remote_install,
                         ExtensionSyncData::OptionalBoolean all_urls_enabled,
                         const syncer::StringOrdinal& app_launch_ordinal,
                         const syncer::StringOrdinal& page_ordinal,
                         extensions::LaunchType launch_type)
    : extension_sync_data_(extension,
                           enabled,
                           disable_reasons,
                           incognito_enabled,
                           remote_install,
                           all_urls_enabled),
      app_launch_ordinal_(app_launch_ordinal),
      page_ordinal_(page_ordinal),
      launch_type_(launch_type) {
  if (extension.from_bookmark()) {
    bookmark_app_description_ = extension.description();
    bookmark_app_url_ = AppLaunchInfo::GetLaunchWebURL(&extension).spec();
    bookmark_app_icon_color_ = AppIconColorInfo::GetIconColorString(&extension);
    extensions::LinkedAppIcons icons =
        LinkedAppIcons::GetLinkedAppIcons(&extension);
    for (const auto& icon : icons.icons) {
      LinkedAppIconInfo linked_icon;
      linked_icon.url = icon.url;
      linked_icon.size = icon.size;
      linked_icons_.push_back(linked_icon);
    }
  }
}

AppSyncData::~AppSyncData() {}

// static
scoped_ptr<AppSyncData> AppSyncData::CreateFromSyncData(
    const syncer::SyncData& sync_data) {
  scoped_ptr<AppSyncData> data(new AppSyncData);
  if (data->PopulateFromSyncData(sync_data))
    return data.Pass();
  return scoped_ptr<AppSyncData>();
}

// static
scoped_ptr<AppSyncData> AppSyncData::CreateFromSyncChange(
    const syncer::SyncChange& sync_change) {
  scoped_ptr<AppSyncData> data(CreateFromSyncData(sync_change.sync_data()));
  if (!data.get())
    return scoped_ptr<AppSyncData>();

  data->extension_sync_data_.set_uninstalled(sync_change.change_type() ==
                                             syncer::SyncChange::ACTION_DELETE);
  return data.Pass();
}

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

  if (!bookmark_app_icon_color_.empty())
    specifics->set_bookmark_app_icon_color(bookmark_app_icon_color_);

  for (const auto& linked_icon : linked_icons_) {
    sync_pb::LinkedAppIconInfo* linked_app_icon_info =
        specifics->add_linked_app_icons();
    linked_app_icon_info->set_url(linked_icon.url.spec());
    linked_app_icon_info->set_size(linked_icon.size);
  }

  extension_sync_data_.PopulateExtensionSpecifics(
      specifics->mutable_extension());
}

bool AppSyncData::PopulateFromAppSpecifics(
    const sync_pb::AppSpecifics& specifics) {
  if (!extension_sync_data_.PopulateFromExtensionSpecifics(
          specifics.extension()))
    return false;

  app_launch_ordinal_ = syncer::StringOrdinal(specifics.app_launch_ordinal());
  page_ordinal_ = syncer::StringOrdinal(specifics.page_ordinal());

  launch_type_ = specifics.has_launch_type()
      ? static_cast<extensions::LaunchType>(specifics.launch_type())
      : LAUNCH_TYPE_INVALID;

  bookmark_app_url_ = specifics.bookmark_app_url();
  bookmark_app_description_ = specifics.bookmark_app_description();
  bookmark_app_icon_color_ = specifics.bookmark_app_icon_color();

  for (int i = 0; i < specifics.linked_app_icons_size(); ++i) {
    const sync_pb::LinkedAppIconInfo& linked_app_icon_info =
        specifics.linked_app_icons(i);
    if (linked_app_icon_info.has_url() && linked_app_icon_info.has_size()) {
      LinkedAppIconInfo linked_icon;
      linked_icon.url = GURL(linked_app_icon_info.url());
      linked_icon.size = linked_app_icon_info.size();
      linked_icons_.push_back(linked_icon);
    }
  }

  return true;
}

bool AppSyncData::PopulateFromSyncData(const syncer::SyncData& sync_data) {
  return PopulateFromAppSpecifics(sync_data.GetSpecifics().app());
}

}  // namespace extensions
