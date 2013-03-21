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

COMPILE_ASSERT(static_cast<sync_pb::CoalescedSyncedNotification_ReadState>(
                   SyncedNotification::kUnread) ==
               sync_pb::CoalescedSyncedNotification_ReadState_UNREAD,
               local_enum_must_match_protobuf_enum);
COMPILE_ASSERT(static_cast<sync_pb::CoalescedSyncedNotification_ReadState>(
                   SyncedNotification::kRead) ==
               sync_pb::CoalescedSyncedNotification_ReadState_READ,
               local_enum_must_match_protobuf_enum);

SyncedNotification::SyncedNotification(const syncer::SyncData& sync_data) {
  Update(sync_data);
}

SyncedNotification::~SyncedNotification() {}

void SyncedNotification::Update(const syncer::SyncData& sync_data) {
  specifics_.CopyFrom(sync_data.GetSpecifics().synced_notification());
}

sync_pb::EntitySpecifics
SyncedNotification::GetEntitySpecifics() const {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_synced_notification()->CopyFrom(specifics_);
  return entity_specifics;
}


std::string SyncedNotification::title() const {
  return ExtractTitle();
}

std::string SyncedNotification::app_id() const {
  return ExtractAppId();
}

std::string SyncedNotification::coalescing_key() const {
  return ExtractCoalescingKey();
}

GURL SyncedNotification::origin_url() const {
  return ExtractOriginUrl();
}

GURL SyncedNotification::icon_url() const {
  return ExtractIconUrl();
}

GURL SyncedNotification::image_url() const {
  return ExtractImageUrl();
}

std::string SyncedNotification::first_external_id() const {
  return ExtractFirstExternalId();
}

std::string SyncedNotification::notification_id() const {
  return ExtractNotificationId();
}

std::string SyncedNotification::body() const {
  return ExtractBody();
}

SyncedNotification::ReadState SyncedNotification::read_state() const {
  return ExtractReadState();
}

// TODO(petewil): Consider the timestamp too once it gets added to the protobuf.
bool SyncedNotification::EqualsIgnoringReadState(
    const SyncedNotification& other) const {
  return (title() == other.title() &&
          app_id() == other.app_id() &&
          coalescing_key() == other.coalescing_key() &&
          first_external_id() == other.first_external_id() &&
          notification_id() == other.notification_id() &&
          body() == other.body() &&
          origin_url() == other.origin_url() &&
          icon_url() == other.icon_url() &&
          image_url() == other.image_url() );
}

bool SyncedNotification::IdMatches(const SyncedNotification& other) const {
  // Two notifications have the same ID if the <appId/coalescingKey> pair
  // matches.
  return (notification_id() == other.notification_id());
}

// Set the read state on the notification, returns true for success.
void SyncedNotification::SetReadState(const ReadState& read_state) {

  // convert the read state to the protobuf type for read state
  if (kRead == read_state)
    specifics_.mutable_coalesced_notification()->
        set_read_state(sync_pb::CoalescedSyncedNotification_ReadState_READ);
  else if (kUnread == read_state)
    specifics_.mutable_coalesced_notification()->
        set_read_state(sync_pb::CoalescedSyncedNotification_ReadState_UNREAD);
  else
    NOTREACHED();
}

// Mark this notification as having been read locally.
void SyncedNotification::NotificationHasBeenRead() {
  SetReadState(kRead);
}

std::string SyncedNotification::ExtractFirstExternalId() const {
  if (!specifics_.has_coalesced_notification() ||
      specifics_.coalesced_notification().notification_size() < 1 ||
      !specifics_.coalesced_notification().notification(0).has_external_id())
    return std::string();

  return specifics_.coalesced_notification().notification(0).external_id();
}

std::string SyncedNotification::ExtractTitle() const {
  if (!specifics_.coalesced_notification().render_info().layout().
      has_layout_type())
    return std::string();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      specifics_.coalesced_notification().render_info().layout().
      layout_type();

  // Depending on the layout type, get the proper title.
  switch (layout_type) {
    case sync_pb::
        SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT: {
      // If we have title and subtext, get that title.
      if (!specifics_.coalesced_notification().render_info().layout().
          title_and_subtext_data().has_title())
        return std::string();

      return specifics_.coalesced_notification().render_info().layout().
          title_and_subtext_data().title();
    }

    case sync_pb::
        SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_IMAGE: {
      // If we have title and image, get that title.
      if (!specifics_.coalesced_notification().render_info().layout().
          title_and_image_data().has_title())
        return std::string();

      return specifics_.coalesced_notification().render_info().layout().
          title_and_image_data().title();
    }
    default: {
      // This is an error case, we should never get here unless the protobuf
      // is bad, or a new type is introduced and this code does not get updated.
      NOTREACHED();
      return std::string();
    }
  }
}

std::string SyncedNotification::ExtractAppId() const {
  if (!specifics_.coalesced_notification().id().has_app_id())
    return std::string();
  return specifics_.coalesced_notification().id().app_id();
}

std::string SyncedNotification::ExtractCoalescingKey() const {
  if (!specifics_.coalesced_notification().id().has_coalescing_key())
    return std::string();
  return specifics_.coalesced_notification().id().coalescing_key();
}

GURL SyncedNotification::ExtractOriginUrl() const {
  std::string origin_url(kExtensionScheme);
  origin_url += app_id();
  return GURL(origin_url);
}

GURL SyncedNotification::ExtractIconUrl() const {
  if (!specifics_.coalesced_notification().render_info().layout().
      has_layout_type())
    return GURL();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      specifics_.coalesced_notification().render_info().layout().
      layout_type();

  // Depending on the layout type, get the icon.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT
      == layout_type) {
    // If we have title and subtext, get that icon.
    if (!specifics_.coalesced_notification().render_info().layout().
        title_and_subtext_data().icon().has_url())
      return GURL();

    return GURL(specifics_.coalesced_notification().render_info().layout().
                title_and_subtext_data().icon().url());
  }
  return GURL();
}

GURL SyncedNotification::ExtractImageUrl() const {
  if (!specifics_.coalesced_notification().render_info().layout().
      has_layout_type())
    return GURL();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      specifics_.coalesced_notification().render_info().layout().
      layout_type();

  // Depending on the layout type, get the image.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_IMAGE
      == layout_type) {
    // If we have title and subtext, get that image.
    if (!specifics_.coalesced_notification().render_info().layout().
        title_and_image_data().image().has_url())
      return GURL();

    return GURL(specifics_.coalesced_notification().render_info().layout().
                title_and_image_data().image().url());
  }
  return GURL();
}

std::string SyncedNotification::ExtractBody() const {
  // If we have subtext data, concatenate the text lines and return it.
  if (!specifics_.coalesced_notification().render_info().layout().
      has_layout_type())
    return std::string();

  const sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType layout_type =
      specifics_.coalesced_notification().render_info().layout().
      layout_type();

  // Check if this layout type includes body text.
  if (sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT
      == layout_type) {
    // If we have title and subtext, get the text.
    if (!specifics_.coalesced_notification().render_info().layout().
        has_title_and_subtext_data())
      return std::string();
    int subtext_lines = specifics_.coalesced_notification().render_info().
        layout().title_and_subtext_data().subtext_size();
    if (subtext_lines < 1)
      return std::string();

    std::string subtext;
    for (int ii = 0; ii < subtext_lines; ++ii) {
      subtext += specifics_.coalesced_notification().render_info().
          layout().
          title_and_subtext_data().subtext(ii);
      if (ii < subtext_lines - 1)
        subtext += '\n';
    }
    return subtext;
  }
  return std::string();
}

SyncedNotification::ReadState SyncedNotification::ExtractReadState() const {
  DCHECK(specifics_.coalesced_notification().has_read_state());

  sync_pb::CoalescedSyncedNotification_ReadState found_read_state =
      specifics_.coalesced_notification().read_state();

  if (found_read_state == sync_pb::CoalescedSyncedNotification_ReadState_READ) {
    return kRead;
  } else if (found_read_state ==
             sync_pb::CoalescedSyncedNotification_ReadState_UNREAD) {
    return kUnread;
  } else {
    NOTREACHED();
    return static_cast<SyncedNotification::ReadState>(found_read_state);
  }
}

std::string SyncedNotification::ExtractNotificationId() const {
  // Append the coalescing key to the app id to get the unique id.
  std::string id = app_id();
  id += "/";
  id += coalescing_key();

  return id;
}

}  // namespace notifier
