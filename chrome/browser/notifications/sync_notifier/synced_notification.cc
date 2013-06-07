// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification_types.h"

namespace {
const char kExtensionScheme[] = "chrome-extension://";

bool UseRichNotifications() {
  return message_center::IsRichNotificationEnabled();
}

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

SyncedNotification::SyncedNotification(const syncer::SyncData& sync_data)
    : notification_manager_(NULL),
      notifier_service_(NULL),
      profile_(NULL),
      active_fetcher_count_(0) {
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

void SyncedNotification::OnFetchComplete(const GURL url,
                                         const SkBitmap* bitmap) {
  // TODO(petewil): Add timeout mechanism in case bitmaps take too long.  Do we
  // already have one built into URLFetcher?
  // Make sure we are on the thread we expect.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Match the incoming bitmaps to URLs.  In case this is a dup, make sure to
  // try all potentially matching urls.
  if (GetAppIconUrl() == url && bitmap != NULL) {
    app_icon_bitmap_ = gfx::Image::CreateFrom1xBitmap(*bitmap);
  }
  if (GetImageUrl() == url && bitmap != NULL) {
    image_bitmap_ = gfx::Image::CreateFrom1xBitmap(*bitmap);
  }
  if (GetButtonOneIconUrl() == url.spec() && bitmap != NULL) {
    button_one_bitmap_ = gfx::Image::CreateFrom1xBitmap(*bitmap);
  }
  if (GetButtonTwoIconUrl() == url.spec() && bitmap != NULL) {
    button_two_bitmap_ = gfx::Image::CreateFrom1xBitmap(*bitmap);
  }

  // Count off the bitmaps as they arrive.
  --active_fetcher_count_;
  DCHECK_GE(active_fetcher_count_, 0);
  // See if all bitmaps are accounted for, if so call Show.
  if (active_fetcher_count_ == 0) {
    Show(notification_manager_, notifier_service_, profile_);
  }
}

void SyncedNotification::QueueBitmapFetchJobs(
    NotificationUIManager* notification_manager,
    ChromeNotifierService* notifier_service,
    Profile* profile) {

  // If we are not using the MessageCenter, call show now, and the existing
  // code will handle the bitmap fetch for us.
  if (!UseRichNotifications()) {
    Show(notification_manager, notifier_service, profile);
    return;
  }

  // Save off the arguments for the call to Show.
  notification_manager_ = notification_manager;
  notifier_service_ = notifier_service;
  profile_ = profile;
  DCHECK_EQ(active_fetcher_count_, 0);

  // Get the URLs that we might need to fetch from Synced Notification.
  // TODO(petewil): clean up the fact that icon and image return a GURL, and
  // button urls return a string.
  // TODO(petewil): Eventually refactor this to accept an arbitrary number of
  // button URLs.

  // If the URL is non-empty, add it to our queue of URLs to fetch.
  AddBitmapToFetchQueue(GetAppIconUrl());
  AddBitmapToFetchQueue(GetImageUrl());
  AddBitmapToFetchQueue(GURL(GetButtonOneIconUrl()));
  AddBitmapToFetchQueue(GURL(GetButtonTwoIconUrl()));

  // If there are no bitmaps, call show now.
  if (active_fetcher_count_ == 0) {
    Show(notification_manager, notifier_service, profile);
  }
}

void SyncedNotification::StartBitmapFetch() {
  // Now that we have queued and counted them all, start the fetching.
  ScopedVector<NotificationBitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    (*iter)->Start(profile_);
  }
}

void SyncedNotification::AddBitmapToFetchQueue(const GURL& url) {
  // Check for dups, ignore any request for a dup.
  ScopedVector<NotificationBitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    if ((*iter)->url() == url)
      return;
  }

  if (url.is_valid()) {
    ++active_fetcher_count_;
    fetchers_.push_back(new NotificationBitmapFetcher(url, this));
  }
}

void SyncedNotification::Show(NotificationUIManager* notification_manager,
                              ChromeNotifierService* notifier_service,
                              Profile* profile) {
  // Set up the fields we need to send and create a Notification object.
  GURL image_url = GetImageUrl();
  string16 text = UTF8ToUTF16(GetText());
  string16 heading = UTF8ToUTF16(GetHeading());
  // TODO(petewil): Eventually put the display name of the sending service here.
  string16 display_source = UTF8ToUTF16(GetOriginUrl().spec());
  string16 replace_key = UTF8ToUTF16(GetKey());

  // The delegate will eventually catch calls that the notification
  // was read or deleted, and send the changes back to the server.
  scoped_refptr<NotificationDelegate> delegate =
      new ChromeNotifierDelegate(GetKey(), notifier_service);

  // TODO(petewil): For now, just punt on dismissed notifications until
  // I change the interface to let NotificationUIManager know the right way.
  if (SyncedNotification::kRead == GetReadState() ||
      SyncedNotification::kDismissed == GetReadState() ) {
    DVLOG(2) << "Dismissed notification arrived"
             << GetHeading() << " " << GetText();
    return;
  }

  // Some inputs and fields are only used if there is a notification center.
  if (UseRichNotifications()) {
    base::Time creation_time =
        base::Time::FromDoubleT(static_cast<double>(GetCreationTime()));
    int priority = GetPriority();
    int notification_count = GetNotificationCount();
    int button_count = GetButtonCount();
    // TODO(petewil): Refactor this for an arbitrary number of buttons.
    std::string button_one_title = GetButtonOneTitle();
    std::string button_two_title = GetButtonTwoTitle();

    // Deduce which notification template to use from the data.
    message_center::NotificationType notification_type =
        message_center::NOTIFICATION_TYPE_BASE_FORMAT;
    if (!image_url.is_empty()) {
      notification_type = message_center::NOTIFICATION_TYPE_IMAGE;
    } else if (notification_count > 1) {
      notification_type = message_center::NOTIFICATION_TYPE_MULTIPLE;
    } else if (button_count > 0) {
      notification_type = message_center::NOTIFICATION_TYPE_BASE_FORMAT;
    }

    // Fill the optional fields with the information we need to make a
    // notification.
    message_center::RichNotificationData rich_notification_data;
    rich_notification_data.timestamp = creation_time;
    if (priority != SyncedNotification::kUndefinedPriority)
      rich_notification_data.priority = priority;
    if (!button_one_title.empty()) {
      message_center::ButtonInfo button_info(UTF8ToUTF16(button_one_title));
      if (!button_one_bitmap_.IsEmpty())
        button_info.icon = button_one_bitmap_;
      rich_notification_data.buttons.push_back(button_info);
    }
    if (!button_two_title.empty()) {
      message_center::ButtonInfo button_info(UTF8ToUTF16(button_two_title));
      if (!button_two_bitmap_.IsEmpty())
        button_info.icon = button_two_bitmap_;
      rich_notification_data.buttons.push_back(button_info);
    }

    // Fill in the bitmap images.
    if (!image_bitmap_.IsEmpty())
      rich_notification_data.image = image_bitmap_;

    // Fill the individual notification fields for a multiple notification.
    if (notification_count > 1) {
      for (int ii = 0; ii < notification_count; ++ii) {
        message_center::NotificationItem item(
            UTF8ToUTF16(GetContainedNotificationTitle(ii)),
            UTF8ToUTF16(GetContainedNotificationMessage(ii)));
        rich_notification_data.items.push_back(item);
      }
    }

    Notification ui_notification(notification_type,
                                 GetOriginUrl(),
                                 heading,
                                 text,
                                 app_icon_bitmap_,
                                 WebKit::WebTextDirectionDefault,
                                 display_source,
                                 replace_key,
                                 rich_notification_data,
                                 delegate.get());
    notification_manager->Add(ui_notification, profile);
  } else {

    Notification ui_notification(GetOriginUrl(),
                                 GetAppIconUrl(),
                                 heading,
                                 text,
                                 WebKit::WebTextDirectionDefault,
                                 display_source,
                                 replace_key,
                                 delegate.get());

    notification_manager->Add(ui_notification, profile);
  }

  DVLOG(1) << "Showing Synced Notification! " << heading << " " << text
           << " " << GetAppIconUrl() << " " << replace_key << " "
           << GetReadState();

  return;
}

// TODO(petewil): Decide what we need for equals - is this enough, or should
// we exhaustively compare every field in case the server refreshed the notif?
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
