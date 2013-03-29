// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification.h"

#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "ui/message_center/notification_types.h"

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
  // TODO(petewil): Let's add checking that the notification looks valid.
  specifics_.CopyFrom(sync_data.GetSpecifics().synced_notification());
}

sync_pb::EntitySpecifics SyncedNotification::GetEntitySpecifics() const {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_synced_notification()->CopyFrom(specifics_);
  return entity_specifics;
}

// TODO(petewil): Consider the timestamp too once it gets added to the protobuf.
// TODO: add more fields in here
bool SyncedNotification::EqualsIgnoringReadState(
    const SyncedNotification& other) const {
  return (GetTitle() == other.GetTitle() &&
          GetAppId() == other.GetAppId() &&
          GetKey() == other.GetKey() &&
          GetText() == other.GetText() &&
          GetOriginUrl() == other.GetOriginUrl() &&
          GetAppIconUrl() == other.GetAppIconUrl() &&
          GetImageUrl() == other.GetImageUrl() );
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

void SyncedNotification::NotificationHasBeenDismissed() {
  SetReadState(kDismissed);
}

std::string SyncedNotification::GetTitle() const {
  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().has_title())
    return std::string();

  return specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().title();
}

std::string SyncedNotification::GetHeading() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_heading())
    return std::string();

  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().heading();
}

std::string SyncedNotification::GetDescription() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_description())
    return std::string();

  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().description();
}

std::string SyncedNotification::GetAppId() const {
  if (!specifics_.coalesced_notification().has_app_id())
    return std::string();
  return specifics_.coalesced_notification().app_id();
}

std::string SyncedNotification::GetKey() const {
  if (!specifics_.coalesced_notification().has_key())
    return std::string();
  return specifics_.coalesced_notification().key();
}

GURL SyncedNotification::GetOriginUrl() const {
  std::string origin_url(kExtensionScheme);
  origin_url += GetAppId();
  return GURL(origin_url);
}

// TODO(petewil): This only returns the first icon. We should make all the
// icons available.
GURL SyncedNotification::GetAppIconUrl() const {
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
GURL SyncedNotification::GetImageUrl() const {
  if (specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().media_size() == 0)
    return GURL();

  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().media(0).image().has_url())
    return GURL();

  return GURL(specifics_.coalesced_notification().render_info().
              expanded_info().simple_expanded_layout().media(0).image().url());
}

std::string SyncedNotification::GetText() const {
  if (!specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().has_text())
    return std::string();

  return specifics_.coalesced_notification().render_info().expanded_info().
      simple_expanded_layout().text();
}

SyncedNotification::ReadState SyncedNotification::GetReadState() const {
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

// Time in milliseconds since the unix epoch, or 0 if not available.
uint64 SyncedNotification::GetCreationTime() const {
  if (!specifics_.coalesced_notification().has_creation_time_msec())
    return 0;

  return specifics_.coalesced_notification().creation_time_msec();
}

int SyncedNotification::GetPriority() const {
  if (!specifics_.coalesced_notification().has_priority())
    return kUndefinedPriority;
  int protobuf_priority = specifics_.coalesced_notification().priority();

#if defined(ENABLE_MESSAGE_CENTER)
  // Convert the prioroty to the scheme used by the notification center.
  if (protobuf_priority ==
      sync_pb::CoalescedSyncedNotification_Priority_LOW) {
    return message_center::LOW_PRIORITY;
  } else if (protobuf_priority ==
             sync_pb::CoalescedSyncedNotification_Priority_STANDARD) {
    return message_center::DEFAULT_PRIORITY;
  } else if (protobuf_priority ==
             sync_pb::CoalescedSyncedNotification_Priority_HIGH) {
    return message_center::HIGH_PRIORITY;
  } else {
    // Complain if this is a new priority we have not seen before.
    DCHECK(protobuf_priority <
           sync_pb::CoalescedSyncedNotification_Priority_LOW  ||
           sync_pb::CoalescedSyncedNotification_Priority_HIGH <
           protobuf_priority);
    return kUndefinedPriority;
  }

#else // ENABLE_MESSAGE_CENTER
  return protobuf_priority;

#endif // ENABLE_MESSAGE_CENTER
}

int SyncedNotification::GetNotificationCount() const {
  return specifics_.coalesced_notification().render_info().
      expanded_info().collapsed_info_size();
}

int SyncedNotification::GetButtonCount() const {
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target_size();
}

std::string SyncedNotification::GetDefaultDestinationTitle() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().has_alt_text()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().alt_text();
}

std::string SyncedNotification::GetDefaultDestinationIconUrl() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().url();
}

std::string SyncedNotification::GetDefaultDestinationUrl() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().url();
}

std::string SyncedNotification::GetButtonOneTitle() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 1)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().icon().has_alt_text()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().icon().alt_text();
}

std::string SyncedNotification::GetButtonOneIconUrl() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 1)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().icon().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().icon().url();
}

std::string SyncedNotification::GetButtonOneUrl() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 1)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(0).action().url();
}

std::string SyncedNotification::GetButtonTwoTitle() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 2)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().icon().has_alt_text()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().icon().alt_text();
}

std::string SyncedNotification::GetButtonTwoIconUrl() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 2)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().icon().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().icon().url();
}

std::string SyncedNotification::GetButtonTwoUrl() const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() < 2)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().has_url()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(1).action().url();
}

std::string SyncedNotification::GetContainedNotificationTitle(
    int index) const {
  if (specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info_size() < index + 1)
    return std::string();

  return specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info(index).simple_collapsed_layout().heading();
}

std::string SyncedNotification::GetContainedNotificationMessage(
    int index) const {
  if (specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info_size() < index + 1)
    return std::string();

  return specifics_.coalesced_notification().render_info().expanded_info().
      collapsed_info(index).simple_collapsed_layout().description();
}

}  // namespace notifier
