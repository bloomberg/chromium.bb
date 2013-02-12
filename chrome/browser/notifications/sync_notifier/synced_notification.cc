// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification.h"

#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"

namespace {
const char kExtensionScheme[] = "chrome-extension://";
}  // namespace

namespace notifier {

// An important side effect of the constructor is that it keeps the
// SyncNotificationSpecifics alive by putting it into the contained sync entity.
// At construction time, copy the data we might need from the sync_data
// protobuf object.
SyncedNotification::SyncedNotification(const syncer::SyncData& sync_data)
    : sync_data_(sync_data),
      has_local_changes_(false),
      title_(ExtractTitle(sync_data)),
      app_id_(ExtractAppId(sync_data)),
      coalescing_key_(ExtractCoalescingKey(sync_data)),
      first_external_id_(ExtractFirstExternalId(sync_data)),
      notification_id_(ExtractNotificationId(sync_data)),
      body_(ExtractBody(sync_data)),
      origin_url_(ExtractOriginUrl(sync_data)),
      icon_url_(ExtractIconUrl(sync_data)),
      image_url_(ExtractImageUrl(sync_data)) {
}

SyncedNotification::~SyncedNotification() {}

const std::string& SyncedNotification::title() const {
  return title_;
}

const std::string& SyncedNotification::app_id() const {
  return app_id_;
}

const std::string& SyncedNotification::coalescing_key() const {
  return coalescing_key_;
}

const GURL& SyncedNotification::origin_url() const {
  return origin_url_;
}

const GURL& SyncedNotification::icon_url() const {
  return icon_url_;
}

const GURL& SyncedNotification::image_url() const {
  return image_url_;
}

const std::string& SyncedNotification::first_external_id() const {
  return first_external_id_;
}

const std::string& SyncedNotification::notification_id() const {
  return notification_id_;
}

const std::string& SyncedNotification::body() const {
  return body_;
}

// TODO(petewil): Consider the timestamp too once it gets added to the protobuf.
bool SyncedNotification::Equals(const SyncedNotification& other) const {
  // Two notifications are equal if the <appId/coalescingKey> pair matches.
  return (notification_id() == other.notification_id());
}

// Set the read state on the notification, returns true for success.
bool SyncedNotification::SetReadState(
    const sync_pb::CoalescedSyncedNotification_ReadState& readState) {

  // TODO(petewil): implement
  return true;
}

// Mark this notification as having been read locally.
void SyncedNotification::NotificationHasBeenRead() {
  // We set the is_local_ flag to true since we modified it locally
  // then we create a sync change object to pass back up.
  has_local_changes_ = true;

  bool success = SetReadState(
      sync_pb::CoalescedSyncedNotification_ReadState_READ);
  DCHECK(success);
}

// mark this notification as having been dismissed locally
void SyncedNotification::NotificationHasBeenDeleted() {
  // We set the is_deleted_ flag to true since we modified it locally
  // then we create a sync change object to pass back up.
  has_local_changes_ = true;

  bool success = SetReadState(
      sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED);
  DCHECK(success);
}

// TODO(petewil): Consider whether the repeated code below can be re-used.
// A first attempt to do so failed.

const sync_pb::SyncedNotificationSpecifics*
SyncedNotification::GetSyncedNotificationSpecifics() {
  return &(sync_data_.GetSpecifics().synced_notification());
}

std::string SyncedNotification::ExtractFirstExternalId(
    const syncer::SyncData& sync_data) const {
  if (!sync_data.GetSpecifics().synced_notification().
      has_coalesced_notification())
    return "";
  if (sync_data.GetSpecifics().synced_notification().
      coalesced_notification().notification_size() < 1)
    return "";
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().notification(0).has_external_id())
    return "";

  return sync_data.GetSpecifics().synced_notification().
      coalesced_notification().notification(0).external_id();
}

std::string SyncedNotification::ExtractTitle(
    const syncer::SyncData& sync_data) const {
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().has_layout_type())
    return "";

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().layout_type();

  // Depending on the layout type, get the proper title.
  switch (layout_type) {
    case sync_pb::
        SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT: {
      // If we have title and subtext, get that title.
      if (!sync_data.GetSpecifics().synced_notification().
          coalesced_notification().render_info().layout().
          title_and_subtext_data().has_title())
        return "";

      return sync_data.GetSpecifics().synced_notification().
          coalesced_notification().render_info().layout().
          title_and_subtext_data().title();
    }

    case sync_pb::
        SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_IMAGE: {
      // If we have title and image, get that title.
      if (!sync_data.GetSpecifics().synced_notification().
          coalesced_notification().render_info().layout().
          title_and_image_data().has_title())
        return "";

      return sync_data.GetSpecifics().synced_notification().
          coalesced_notification().render_info().layout().
          title_and_image_data().title();
    }
    default: {
      // This is an error case, we should never get here unless the protobuf
      // is bad, or a new type is introduced and this code does not get updated.
      NOTREACHED();
      return "";
    }
  }
}

std::string SyncedNotification::ExtractAppId(
    const syncer::SyncData& sync_data) const {
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().id().
      has_app_id())
    return "";
  return sync_data.GetSpecifics().synced_notification().
      coalesced_notification().id().app_id();
}

std::string SyncedNotification::ExtractCoalescingKey(
    const syncer::SyncData& sync_data) const {
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().id().
      has_coalescing_key())
    return "";
  return sync_data.GetSpecifics().synced_notification().
      coalesced_notification().id().coalescing_key();
}

GURL SyncedNotification::ExtractOriginUrl(
    const syncer::SyncData& sync_data) const {
  std::string origin_url(kExtensionScheme);
  origin_url += app_id_;
  return GURL(origin_url);
}

GURL SyncedNotification::ExtractIconUrl(const syncer::SyncData& sync_data)
    const {
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().has_layout_type())
    return GURL();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().layout_type();

  // Depending on the layout type, get the icon.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT
      == layout_type) {
    // If we have title and subtext, get that icon.
    if (!sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
        title_and_subtext_data().icon().has_url())
      return GURL();

    return GURL(sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
                title_and_subtext_data().icon().url());
  }
  return GURL();
}

GURL SyncedNotification::ExtractImageUrl(const syncer::SyncData& sync_data)
    const {
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().has_layout_type())
    return GURL();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().layout_type();

  // Depending on the layout type, get the image.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_IMAGE
      == layout_type) {
    // If we have title and subtext, get that image.
    if (!sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
        title_and_image_data().image().has_url())
      return GURL();

    return GURL(sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
                title_and_image_data().image().url());
  }
  return GURL();
}

std::string SyncedNotification::ExtractBody(
    const syncer::SyncData& sync_data) const {
  // If we have subtext data, concatenate the text lines and return it.
  if (!sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().has_layout_type())
    return "";

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      sync_data.GetSpecifics().synced_notification().
      coalesced_notification().render_info().layout().layout_type();

  // Check if this layout type includes body text.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT
      == layout_type) {
    // If we have title and subtext, get the text.
    if (!sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
        has_title_and_subtext_data())
      return "";
    int subtext_lines = sync_data.GetSpecifics().synced_notification().
        coalesced_notification().render_info().layout().
        title_and_subtext_data().subtext_size();
    if (subtext_lines < 1)
      return "";

    std::string subtext;
    for (int ii = 0; ii < subtext_lines; ++ii) {
      subtext += sync_data.GetSpecifics().synced_notification().
          coalesced_notification().render_info().layout().
          title_and_subtext_data().subtext(ii);
      if (ii < subtext_lines - 1)
        subtext += '\n';
    }
    return subtext;
  }
  return "";
}

std::string SyncedNotification::ExtractNotificationId(
    const syncer::SyncData& sync_data) const {
  // Append the coalescing key to the app id to get the unique id.
  std::string id = app_id_;
  id += "/";
  id += coalescing_key_;

  return id;
}

}  // namespace notifier
