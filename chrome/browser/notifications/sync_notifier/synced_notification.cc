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
COMPILE_ASSERT(static_cast<sync_pb::CoalescedSyncedNotification_ReadState>(
                   SyncedNotification::kDismissed) ==
               sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED,
               local_enum_must_match_protobuf_enum);

SyncedNotification::SyncedNotification(const syncer::SyncData& sync_data) {
  Update(sync_data);
}

SyncedNotification::~SyncedNotification() {}

void SyncedNotification::Update(const syncer::SyncData& sync_data) {
  specifics_.CopyFrom(sync_data.GetSpecifics().synced_notification());
}

sync_pb::EntitySpecifics SyncedNotification::GetEntitySpecifics() const {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_synced_notification()->CopyFrom(specifics_);
  return entity_specifics;
}

std::string SyncedNotification::title() const {
  return ExtractTitle();
}

std::string SyncedNotification::heading() const {
  return ExtractHeading();
}

std::string SyncedNotification::description() const {
  return ExtractDescription();
}

std::string SyncedNotification::app_id() const {
  return ExtractAppId();
}

std::string SyncedNotification::key() const {
  return ExtractKey();
}

GURL SyncedNotification::origin_url() const {
  return ExtractOriginUrl();
}

GURL SyncedNotification::app_icon_url() const {
  return ExtractAppIconUrl();
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

std::string SyncedNotification::text() const {
  return ExtractText();
}

SyncedNotification::ReadState SyncedNotification::read_state() const {
  return ExtractReadState();
}

// TODO(petewil): Consider the timestamp too once it gets added to the protobuf.
bool SyncedNotification::EqualsIgnoringReadState(
    const SyncedNotification& other) const {
  return (title() == other.title() &&
          app_id() == other.app_id() &&
          key() == other.key() &&
          text() == other.text() &&
          origin_url() == other.origin_url() &&
          app_icon_url() == other.app_icon_url() &&
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
  if (kDismissed == read_state)
    specifics_.mutable_coalesced_notification()->set_read_state(
        sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED);
  else if (kUnread == read_state)
    specifics_.mutable_coalesced_notification()->set_read_state(
        sync_pb::CoalescedSyncedNotification_ReadState_UNREAD);
  else if (kRead == read_state)
    specifics_.mutable_coalesced_notification()->set_read_state(
        sync_pb::CoalescedSyncedNotification_ReadState_READ);
  else
    NOTREACHED();
}

void SyncedNotification::NotificationHasBeenRead() {
  SetReadState(kRead);
}

void SyncedNotification::NotificationHasBeenDismissed() {
  SetReadState(kDismissed);
}

std::string SyncedNotification::ExtractFirstExternalId() const {
  if (!specifics_.has_coalesced_notification() ||
      specifics_.coalesced_notification().notification_size() < 1 ||
      !specifics_.coalesced_notification().notification(0).has_external_id())
    return std::string();

  return specifics_.coalesced_notification().notification(0).external_id();
}

std::string SyncedNotification::ExtractTitle() const {
  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().has_title())
    return "";

  return specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().title();
}

std::string SyncedNotification::ExtractHeading() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_heading())
    return "";

  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().heading();
}

std::string SyncedNotification::ExtractDescription() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_description())
    return "";

  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().description();
}

std::string SyncedNotification::ExtractAppId() const {
  if (!specifics_.coalesced_notification().has_app_id())
    return "";
  return specifics_.coalesced_notification().app_id();
}

std::string SyncedNotification::ExtractKey() const {
  if (!specifics_.coalesced_notification().has_key())
    return "";
  return specifics_.coalesced_notification().key();
}

GURL SyncedNotification::ExtractOriginUrl() const {
  std::string origin_url(kExtensionScheme);
  origin_url += app_id();
  return GURL(origin_url);
}

GURL SyncedNotification::ExtractAppIconUrl() const {
  if (specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info_size() == 0)
    return GURL();

  if (!specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info(0).simple_collapsed_layout().has_app_icon())
    return GURL();

  return GURL(specifics_.coalesced_notification().render_info().
              expanded_info().collapsed_info(0).simple_collapsed_layout().
              app_icon().url());
}

// TODO(petewil): This currenly only handles the first image from the first
// collapsed item, someday return all images.
GURL SyncedNotification::ExtractImageUrl() const {
  if (specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().media_size() == 0)
    return GURL();

  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().media(0).image().has_url())
    return GURL();

  return GURL(specifics_.coalesced_notification().render_info().
              expanded_info().simple_expanded_layout().media(0).image().url());
}

std::string SyncedNotification::ExtractText() const {
  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().has_text())
    return "";

  return specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().text();
}

SyncedNotification::ReadState SyncedNotification::ExtractReadState() const {
  DCHECK(specifics_.coalesced_notification().has_read_state());

  sync_pb::CoalescedSyncedNotification_ReadState found_read_state =
      specifics_.coalesced_notification().read_state();

  if (found_read_state ==
      sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED) {
    return kDismissed;
  } else if (found_read_state ==
             sync_pb::CoalescedSyncedNotification_ReadState_UNREAD) {
    return kUnread;
  } else if (found_read_state ==
             sync_pb::CoalescedSyncedNotification_ReadState_READ) {
    return kRead;
  } else {
    NOTREACHED();
    return static_cast<SyncedNotification::ReadState>(found_read_state);
  }
}

std::string SyncedNotification::ExtractNotificationId() const {
  return key();
}

}  // namespace notifier
