// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification.h"

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification_types.h"

namespace {
const char kExtensionScheme[] = "synced-notification://";
const char kDefaultSyncedNotificationScheme[] = "https:";

// Today rich notifications only supports two buttons, make sure we don't
// try to supply them with more than this number of buttons.
const unsigned int kMaxNotificationButtonIndex = 2;

// Schema-less specs default badly in windows.  If we find one, add the schema
// we expect instead of allowing windows specific GURL code to make it default
// to "file:".
GURL AddDefaultSchemaIfNeeded(std::string& url_spec) {
  if (StartsWithASCII(url_spec, std::string("//"), false))
    return GURL(std::string(kDefaultSyncedNotificationScheme) + url_spec);

  return GURL(url_spec);
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

SyncedNotification::SyncedNotification(
    const syncer::SyncData& sync_data,
    ChromeNotifierService* notifier_service,
    NotificationUIManager* notification_manager)
    : notification_manager_(notification_manager),
      notifier_service_(notifier_service),
      profile_(NULL),
      toast_state_(true),
      app_icon_bitmap_fetch_pending_(true),
      sender_bitmap_fetch_pending_(true),
      image_bitmap_fetch_pending_(true) {
  Update(sync_data);
}

SyncedNotification::~SyncedNotification() {}

void SyncedNotification::Update(const syncer::SyncData& sync_data) {
  // TODO(petewil): Add checking that the notification looks valid.
  specifics_.CopyFrom(sync_data.GetSpecifics().synced_notification());
}

void SyncedNotification::Show(Profile* profile) {
  // Let NotificationUIManager know that the notification has been dismissed.
  if (SyncedNotification::kRead == GetReadState() ||
      SyncedNotification::kDismissed == GetReadState() ) {
    notification_manager_->CancelById(GetKey());
    DVLOG(2) << "Dismissed or read notification arrived"
             << GetHeading() << " " << GetText();
    return;
  }

  // |notifier_service| can be NULL in tests.
  if (notifier_service_) {
    notifier_service_->ShowWelcomeToastIfNecessary(this, notification_manager_);
  }

  // Set up the fields we need to send and create a Notification object.
  GURL image_url = GetImageUrl();
  base::string16 text = base::UTF8ToUTF16(GetText());
  base::string16 heading = base::UTF8ToUTF16(GetHeading());
  base::string16 description = base::UTF8ToUTF16(GetDescription());
  base::string16 annotation = base::UTF8ToUTF16(GetAnnotation());
  // TODO(petewil): Eventually put the display name of the sending service here.
  base::string16 display_source = base::UTF8ToUTF16(GetAppId());
  base::string16 replace_key = base::UTF8ToUTF16(GetKey());
  base::string16 notification_heading = heading;
  base::string16 notification_text = description;
  base::string16 newline = base::UTF8ToUTF16("\n");

  // The delegate will eventually catch calls that the notification
  // was read or deleted, and send the changes back to the server.
  scoped_refptr<NotificationDelegate> delegate =
      new ChromeNotifierDelegate(GetKey(), notifier_service_);

  // Some inputs and fields are only used if there is a notification center.
  base::Time creation_time =
      base::Time::FromDoubleT(static_cast<double>(GetCreationTime()));
  int priority = GetPriority();
  unsigned int button_count = GetButtonCount();

  // Deduce which notification template to use from the data.
  message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_BASE_FORMAT;
  if (!image_url.is_empty()) {
    notification_type = message_center::NOTIFICATION_TYPE_IMAGE;
  } else if (button_count > 0) {
    notification_type = message_center::NOTIFICATION_TYPE_BASE_FORMAT;
  }

  // Fill the optional fields with the information we need to make a
  // notification.
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.timestamp = creation_time;
  if (priority != SyncedNotification::kUndefinedPriority)
    rich_notification_data.priority = priority;

  // Fill in the button data.
  // TODO(petewil): Today Rich notifiations are limited to two buttons.
  // When rich notifications supports more, remove the
  // "&& i < kMaxNotificationButtonIndex" clause below.
  for (unsigned int i = 0;
        i < button_count
        && i < button_bitmaps_.size()
        && i < kMaxNotificationButtonIndex;
        ++i) {
    // Stop at the first button with no title
    std::string title = GetButtonTitle(i);
    if (title.empty())
      break;
    message_center::ButtonInfo button_info(base::UTF8ToUTF16(title));
    if (!button_bitmaps_[i].IsEmpty())
      button_info.icon = button_bitmaps_[i];
    rich_notification_data.buttons.push_back(button_info);
  }

  // Fill in the bitmap images.
  if (!image_bitmap_.IsEmpty())
    rich_notification_data.image = image_bitmap_;

  if (!app_icon_bitmap_.IsEmpty()) {
    // Since we can't control the size of images we download, resize using a
    // high quality filter down to the appropriate icon size.
    // TODO(dewittj): Remove this when correct resources are sent via the
    // protobuf.
    SkBitmap new_app_icon =
        skia::ImageOperations::Resize(app_icon_bitmap_.AsBitmap(),
                                      skia::ImageOperations::RESIZE_BEST,
                                      message_center::kSmallImageSize,
                                      message_center::kSmallImageSize);

    // The app icon should be in grayscale.
    // TODO(dewittj): Remove this when correct resources are sent via the
    // protobuf.
    color_utils::HSL shift = {-1, 0, 0.6};
    SkBitmap grayscale =
        SkBitmapOperations::CreateHSLShiftedBitmap(new_app_icon, shift);
    gfx::Image small_image =
        gfx::Image(gfx::ImageSkia(gfx::ImageSkiaRep(grayscale, 1.0f)));
    rich_notification_data.small_image = small_image;
  }

  // Set the ContextMessage inside the rich notification data for the
  // annotation.
  rich_notification_data.context_message = annotation;

  // Set the clickable flag to change the cursor on hover if a valid
  // destination is found.
  rich_notification_data.clickable = GetDefaultDestinationUrl().is_valid();

  // If there is at least one person sending, use the first picture.
  // TODO(petewil): Someday combine multiple profile photos here.
  gfx::Image icon_bitmap = app_icon_bitmap_;
  if (GetProfilePictureCount() >= 1)  {
    icon_bitmap = sender_bitmap_;
  }

  Notification ui_notification(notification_type,
                                GetOriginUrl(),
                                notification_heading,
                                notification_text,
                                icon_bitmap,
                                blink::WebTextDirectionDefault,
                                message_center::NotifierId(GetOriginUrl()),
                                display_source,
                                replace_key,
                                rich_notification_data,
                                delegate.get());
  // In case the notification is not supposed to be toasted, pretend that it
  // has already been shown.
  ui_notification.set_shown_as_popup(!toast_state_);

  notification_manager_->Add(ui_notification, profile);

  DVLOG(1) << "Showing Synced Notification! " << heading << " " << text
           << " " << GetAppIconUrl() << " " << replace_key << " "
           << GetProfilePictureUrl(0) << " " << GetReadState();

  return;
}

sync_pb::EntitySpecifics SyncedNotification::GetEntitySpecifics() const {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_synced_notification()->CopyFrom(specifics_);
  return entity_specifics;
}

// Display the notification if it has the specified app_id_name.
void SyncedNotification::ShowAllForAppId(Profile* profile,
                                         std::string app_id_name) {
  if (app_id_name == GetAppId())
    Show(profile);
}

// Remove the notification if it has the specified app_id_name.
void SyncedNotification::HideAllForAppId(std::string app_id_name) {
  if (app_id_name == GetAppId()) {
    notification_manager_->CancelById(GetKey());
  }
}

void SyncedNotification::QueueBitmapFetchJobs(
    ChromeNotifierService* notifier_service,
    Profile* profile) {
  // Save off the arguments for the call to Show.
  notifier_service_ = notifier_service;
  profile_ = profile;

  // Ensure our bitmap vector has as many entries as there are buttons,
  // so that when the bitmaps arrive the vector has a slot for them.
  for (unsigned int i = 0; i < GetButtonCount(); ++i) {
    button_bitmaps_.push_back(gfx::Image());
    button_bitmaps_fetch_pending_.push_back(true);
    CreateBitmapFetcher(GetButtonIconUrl(i));
  }

  // If there is a profile image bitmap, fetch it
  if (GetProfilePictureCount() > 0) {
    // TODO(petewil): When we have the capacity to display more than one bitmap,
    // modify this code to fetch as many as we can display
    CreateBitmapFetcher(GetProfilePictureUrl(0));
  }

  // If the URL is non-empty, add it to our queue of URLs to fetch.
  CreateBitmapFetcher(GetAppIconUrl());
  CreateBitmapFetcher(GetImageUrl());

  // Check to see if we don't need to fetch images, either because we already
  // did, or because the URLs are empty. If so, we can display the notification.

  // See if all bitmaps are accounted for, if so call Show().
  if (AreAllBitmapsFetched()) {
    Show(profile_);
  }
}

void SyncedNotification::StartBitmapFetch() {
  // Now that we have queued and counted them all, start the fetching.
  ScopedVector<chrome::BitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    (*iter)->Start(profile_);
  }
}

// This should detect even small changes in case the server updated the
// notification.  We ignore the timestamp if other fields match.
bool SyncedNotification::EqualsIgnoringReadState(
    const SyncedNotification& other) const {
  if (GetTitle() == other.GetTitle() &&
      GetHeading() == other.GetHeading() &&
      GetDescription() == other.GetDescription() &&
      GetAnnotation() == other.GetAnnotation() &&
      GetAppId() == other.GetAppId() &&
      GetKey() == other.GetKey() &&
      GetOriginUrl() == other.GetOriginUrl() &&
      GetAppIconUrl() == other.GetAppIconUrl() &&
      GetImageUrl() == other.GetImageUrl() &&
      GetText() == other.GetText() &&
      // We intentionally skip read state
      GetCreationTime() == other.GetCreationTime() &&
      GetPriority() == other.GetPriority() &&
      GetDefaultDestinationTitle() == other.GetDefaultDestinationTitle() &&
      GetDefaultDestinationIconUrl() == other.GetDefaultDestinationIconUrl() &&
      GetNotificationCount() == other.GetNotificationCount() &&
      GetButtonCount() == other.GetButtonCount() &&
      GetProfilePictureCount() == other.GetProfilePictureCount()) {

    // If all the surface data matched, check, to see if contained data also
    // matches, titles and messages.
    size_t count = GetNotificationCount();
    for (size_t ii = 0; ii < count; ++ii) {
      if (GetContainedNotificationTitle(ii) !=
          other.GetContainedNotificationTitle(ii))
        return false;
      if (GetContainedNotificationMessage(ii) !=
          other.GetContainedNotificationMessage(ii))
        return false;
    }

    // Make sure buttons match.
    count = GetButtonCount();
    for (size_t jj = 0; jj < count; ++jj) {
      if (GetButtonTitle(jj) != other.GetButtonTitle(jj))
        return false;
      if (GetButtonIconUrl(jj) != other.GetButtonIconUrl(jj))
        return false;
    }

    // Make sure profile icons match
    count = GetButtonCount();
    for (size_t kk = 0; kk < count; ++kk) {
      if (GetProfilePictureUrl(kk) != other.GetProfilePictureUrl(kk))
        return false;
    }

    // If buttons and notifications matched, they are equivalent.
    return true;
  }

  return false;
}

void SyncedNotification::NotificationHasBeenRead() {
  SetReadState(kRead);
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

std::string SyncedNotification::GetAnnotation() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_annotation())
    return std::string();

  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().annotation();
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

GURL SyncedNotification::GetAppIconUrl() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().has_app_icon())
    return GURL();

  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().simple_collapsed_layout().app_icon().url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

// TODO(petewil): This ignores all but the first image.  If Rich Notifications
// supports more images someday, then fetch all images.
GURL SyncedNotification::GetImageUrl() const {
  if (specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().media_size() == 0)
    return GURL();

  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().media(0).image().has_url())
    return GURL();

  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().simple_collapsed_layout().media(0).image().url();

  return AddDefaultSchemaIfNeeded(url_spec);
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
      sync_pb::CoalescedSyncedNotification_Priority_INVISIBLE) {
    return message_center::LOW_PRIORITY;
  } else if (protobuf_priority ==
             sync_pb::CoalescedSyncedNotification_Priority_LOW) {
    return message_center::DEFAULT_PRIORITY;
  } else if (protobuf_priority ==
             sync_pb::CoalescedSyncedNotification_Priority_HIGH) {
    return message_center::HIGH_PRIORITY;
  } else {
    // Complain if this is a new priority we have not seen before.
    DCHECK(protobuf_priority <
           sync_pb::CoalescedSyncedNotification_Priority_INVISIBLE  ||
           sync_pb::CoalescedSyncedNotification_Priority_HIGH <
           protobuf_priority);
    return kUndefinedPriority;
  }
}

std::string SyncedNotification::GetDefaultDestinationTitle() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().has_alt_text()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().alt_text();
}

GURL SyncedNotification::GetDefaultDestinationIconUrl() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().icon().has_url()) {
    return GURL();
  }
  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().default_destination().icon().url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

GURL SyncedNotification::GetDefaultDestinationUrl() const {
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      default_destination().has_url()) {
    return GURL();
  }
  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().default_destination().url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

std::string SyncedNotification::GetButtonTitle(
    unsigned int which_button) const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() <= which_button)
    return std::string();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(which_button).action().icon().has_alt_text()) {
    return std::string();
  }
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target(which_button).action().icon().alt_text();
}

GURL SyncedNotification::GetButtonIconUrl(unsigned int which_button) const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() <= which_button)
    return GURL();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(which_button).action().icon().has_url()) {
    return GURL();
  }
  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().target(which_button).action().icon().url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

GURL SyncedNotification::GetButtonUrl(unsigned int which_button) const {
  // Must ensure that we have a target before trying to access it.
  if (GetButtonCount() <= which_button)
    return GURL();
  if (!specifics_.coalesced_notification().render_info().collapsed_info().
      target(which_button).action().has_url()) {
    return GURL();
  }
  std::string url_spec = specifics_.coalesced_notification().render_info().
              collapsed_info().target(which_button).action().url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

GURL SyncedNotification::GetProfilePictureUrl(unsigned int which_url) const {
  if (GetProfilePictureCount() <= which_url)
    return GURL();

  std::string url_spec = specifics_.coalesced_notification().render_info().
      collapsed_info().simple_collapsed_layout().profile_image(which_url).
      image_url();

  return AddDefaultSchemaIfNeeded(url_spec);
}

size_t SyncedNotification::GetProfilePictureCount() const {
  return specifics_.coalesced_notification().render_info().collapsed_info().
      simple_collapsed_layout().profile_image_size();
}

size_t SyncedNotification::GetNotificationCount() const {
  return specifics_.coalesced_notification().render_info().
      expanded_info().collapsed_info_size();
}

size_t SyncedNotification::GetButtonCount() const {
  return specifics_.coalesced_notification().render_info().collapsed_info().
      target_size();
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

const gfx::Image& SyncedNotification::GetAppIcon() const {
  return app_icon_bitmap_;
}

void SyncedNotification::set_toast_state(bool toast_state) {
  toast_state_ = toast_state;
}

void SyncedNotification::LogNotification() {
  std::string readStateString("Unread");
  if (SyncedNotification::kRead == GetReadState())
    readStateString = "Read";
  else if (SyncedNotification::kDismissed == GetReadState())
    readStateString = "Dismissed";

  DVLOG(2) << " Notification: Heading is " << GetHeading()
           << " description is " << GetDescription()
           << " key is " << GetKey()
           << " read state is " << readStateString;
}

// TODO(petewil): The fetch mechanism appears to be returning two bitmaps on the
// mac - perhaps one is regular, one is high dpi?  If so, ensure we use the high
// dpi bitmap when appropriate.
void SyncedNotification::OnFetchComplete(const GURL url,
                                         const SkBitmap* bitmap) {
  // Make sure we are on the thread we expect.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  gfx::Image downloaded_image;
  if (bitmap != NULL)
    downloaded_image = gfx::Image::CreateFrom1xBitmap(*bitmap);

  // Match the incoming bitmaps to URLs.  In case this is a dup, make sure to
  // try all potentially matching urls.
  if (GetAppIconUrl() == url) {
    app_icon_bitmap_ = downloaded_image;
    if (app_icon_bitmap_.IsEmpty())
      app_icon_bitmap_fetch_pending_ = false;
  }
  if (GetImageUrl() == url) {
    image_bitmap_ = downloaded_image;
    if (image_bitmap_.IsEmpty())
      image_bitmap_fetch_pending_ = false;
  }
  if (GetProfilePictureUrl(0) == url) {
    sender_bitmap_ = downloaded_image;
    if (sender_bitmap_.IsEmpty())
      sender_bitmap_fetch_pending_ = false;
  }

  // If this URL matches one or more button bitmaps, save them off.
  for (unsigned int i = 0; i < GetButtonCount(); ++i) {
    if (GetButtonIconUrl(i) == url) {
      if (bitmap != NULL) {
        button_bitmaps_[i] = gfx::Image::CreateFrom1xBitmap(*bitmap);
      }
      button_bitmaps_fetch_pending_[i] = false;
    }
  }

  DVLOG(2) << __FUNCTION__ << " popping bitmap " << url;

  // See if all bitmaps are already accounted for, if so call Show.
  if (AreAllBitmapsFetched()) {
    Show(profile_);
  }
}

void SyncedNotification::CreateBitmapFetcher(const GURL& url) {
  // Check for dups, ignore any request for a dup.
  ScopedVector<chrome::BitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    if ((*iter)->url() == url)
      return;
  }

  if (url.is_valid()) {
    fetchers_.push_back(new chrome::BitmapFetcher(url, this));
    DVLOG(2) << __FUNCTION__ << "Pushing bitmap " << url;
  }
}

// Check that we have either fetched or gotten an error on all the bitmaps we
// asked for.
bool SyncedNotification::AreAllBitmapsFetched() {
  bool app_icon_ready = GetAppIconUrl().is_empty() ||
      !app_icon_bitmap_.IsEmpty() || !app_icon_bitmap_fetch_pending_;
  bool images_ready = GetImageUrl().is_empty() || !image_bitmap_.IsEmpty() ||
      !image_bitmap_fetch_pending_;
  bool sender_picture_ready = GetProfilePictureUrl(0).is_empty() ||
      !sender_bitmap_.IsEmpty() || !sender_bitmap_fetch_pending_;
  bool button_bitmaps_ready = true;
  for (unsigned int j = 0; j < GetButtonCount(); ++j) {
    if (!GetButtonIconUrl(j).is_empty()
        && button_bitmaps_[j].IsEmpty()
        && button_bitmaps_fetch_pending_[j]) {
      button_bitmaps_ready = false;
      break;
    }
  }

  return app_icon_ready && images_ready && sender_picture_ready &&
      button_bitmaps_ready;
}

// Set the read state on the notification, returns true for success.
void SyncedNotification::SetReadState(const ReadState& read_state) {

  // Convert the read state to the protobuf type for read state.
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

}  // namespace notifier
