// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ChromeNotifierService works together with sync to maintain the state of
// user notifications, which can then be presented in the notification center,
// via the Notification UI Manager.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "googleurl/src/gurl.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "ui/message_center/notification_types.h"

namespace notifier {

ChromeNotifierService::ChromeNotifierService(Profile* profile,
                                             NotificationUIManager* manager)
    : profile_(profile), notification_manager_(manager) {}
ChromeNotifierService::~ChromeNotifierService() {}

// Methods from ProfileKeyedService.
void ChromeNotifierService::Shutdown() {
}

// syncer::SyncableService implementation.

// This is called at startup to sync with the server.
// This code is not thread safe.
syncer::SyncMergeResult ChromeNotifierService::MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  // TODO(petewil): After I add the infrastructure for the test, add a check
  // that we are currently on the UI thread here.
  DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, type);
  syncer::SyncMergeResult merge_result(syncer::SYNCED_NOTIFICATIONS);
  // A list of local changes to send up to the sync server.
  syncer::SyncChangeList new_changes;
  sync_processor_ = sync_processor.Pass();

  for (syncer::SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    const syncer::SyncData& sync_data = *it;
    DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, sync_data.GetDataType());

    // Build a local notification object from the sync data.
    scoped_ptr<SyncedNotification> incoming(CreateNotificationFromSyncData(
        sync_data));
    DCHECK(incoming.get());

    // Process each incoming remote notification.
    const std::string& key = incoming->GetKey();
    DCHECK_GT(key.length(), 0U);
    SyncedNotification* found = FindNotificationByKey(key);

    if (NULL == found) {
      // If there are no conflicts, copy in the data from remote.
      Add(incoming.Pass());
    } else {
      // If the incoming (remote) and stored (local) notifications match
      // in all fields, we don't need to do anything here.
      if (incoming->EqualsIgnoringReadState(*found)) {

        if (incoming->GetReadState() == found->GetReadState()) {
          // Notification matches on the client and the server, nothing to do.
          continue;
        } else  {
          // If the read state is different, read wins for both places.
          if (incoming->GetReadState() == SyncedNotification::kDismissed) {
            // If it is marked as read on the server, but not the client.
            found->NotificationHasBeenDismissed();
            // TODO(petewil): Tell the Notification UI Manager to mark it read.
          } else {
            // If it is marked as read on the client, but not the server.
            syncer::SyncData sync_data = CreateSyncDataFromNotification(*found);
            new_changes.push_back(
                syncer::SyncChange(FROM_HERE,
                                   syncer::SyncChange::ACTION_UPDATE,
                                   sync_data));
          }
          // If local state changed, notify Notification UI Manager.
        }
      // For any other conflict besides read state, treat it as an update.
      } else {
        // If different, just replace the local with the remote.
        // TODO(petewil): Someday we may allow changes from the client to
        // flow upwards, when we do, we will need better merge resolution.
        found->Update(sync_data);
      }
    }
  }

  // Send up the changes that were made locally.
  if (new_changes.size() > 0) {
    merge_result.set_error(sync_processor_->ProcessSyncChanges(
        FROM_HERE, new_changes));
  }

  return merge_result;
}

void ChromeNotifierService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, type);
  // TODO(petewil): implement
}

syncer::SyncDataList ChromeNotifierService::GetAllSyncData(
      syncer::ModelType type) const {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, type);
  syncer::SyncDataList sync_data;

  // Copy our native format data into a SyncDataList format.
  for (std::vector<SyncedNotification*>::const_iterator it =
          notification_data_.begin();
      it != notification_data_.end();
      ++it) {
    sync_data.push_back(CreateSyncDataFromNotification(**it));
  }

  return sync_data;
}

// This method is called when there is an incoming sync change from the server.
syncer::SyncError ChromeNotifierService::ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) {
  // TODO(petewil): add a check that we are called on the thread we expect.
  syncer::SyncError error;

  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    syncer::SyncData sync_data = it->sync_data();
    DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, sync_data.GetDataType());
    syncer::SyncChange::SyncChangeType change_type = it->change_type();

    scoped_ptr<SyncedNotification> new_notification(
        CreateNotificationFromSyncData(sync_data));
    if (!new_notification.get()) {
      NOTREACHED() << "Failed to read notification.";
      continue;
    }

    switch (change_type) {
      case syncer::SyncChange::ACTION_ADD:
        // TODO(petewil): Update the notification if it already exists
        // as opposed to adding it.
        Add(new_notification.Pass());
        break;
      // TODO(petewil): Implement code to add delete and update actions.

      default:
        break;
    }
  }

  return error;
}

// Support functions for data type conversion.

// Static method.  Get to the sync data in our internal format.
syncer::SyncData ChromeNotifierService::CreateSyncDataFromNotification(
    const SyncedNotification& notification) {
  // Construct the sync_data using the specifics from the notification.
  return syncer::SyncData::CreateLocalData(
      notification.GetKey(), notification.GetKey(),
      notification.GetEntitySpecifics());
}

// Static Method.  Convert from SyncData to our internal format.
scoped_ptr<SyncedNotification>
    ChromeNotifierService::CreateNotificationFromSyncData(
        const syncer::SyncData& sync_data) {
  // Get a pointer to our data within the sync_data object.
  sync_pb::SyncedNotificationSpecifics specifics =
      sync_data.GetSpecifics().synced_notification();

  // Check for mandatory fields in the sync_data object.
  if (!specifics.has_coalesced_notification() ||
      !specifics.coalesced_notification().has_key() ||
      !specifics.coalesced_notification().has_read_state())
    return scoped_ptr<SyncedNotification>();

  // TODO(petewil): Is this the right set?  Should I add more?
  bool is_well_formed_unread_notification =
      (static_cast<SyncedNotification::ReadState>(
          specifics.coalesced_notification().read_state()) ==
       SyncedNotification::kUnread &&
       specifics.coalesced_notification().has_render_info());
  bool is_well_formed_dismissed_notification =
      (static_cast<SyncedNotification::ReadState>(
          specifics.coalesced_notification().read_state()) ==
       SyncedNotification::kDismissed);

  // if the notification is poorly formed, return a null pointer
  if (!is_well_formed_unread_notification &&
      !is_well_formed_dismissed_notification)
    return scoped_ptr<SyncedNotification>();

  // Create a new notification object based on the supplied sync_data.
  scoped_ptr<SyncedNotification> notification(
      new SyncedNotification(sync_data));

  return notification.Pass();
}

// This returns a pointer into a vector that we own.  Caller must not free it.
// Returns NULL if no match is found.
SyncedNotification* ChromeNotifierService::FindNotificationByKey(
    const std::string& key) {
  // TODO(petewil): We can make a performance trade off here.
  // While the vector has good locality of reference, a map has faster lookup.
  // Based on how big we expect this to get, maybe change this to a map.
  for (std::vector<SyncedNotification*>::const_iterator it =
          notification_data_.begin();
      it != notification_data_.end();
      ++it) {
    SyncedNotification* notification = *it;
    if (key == notification->GetKey())
      return *it;
  }

  return NULL;
}

void ChromeNotifierService::MarkNotificationAsDismissed(
    const std::string& key) {
  SyncedNotification* notification = FindNotificationByKey(key);
  CHECK(notification != NULL);

  notification->NotificationHasBeenDismissed();
  syncer::SyncChangeList new_changes;

  syncer::SyncData sync_data = CreateSyncDataFromNotification(*notification);
  new_changes.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         sync_data));

  // Send up the changes that were made locally.
  sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
}

// Add a new notification to our data structure.  This takes ownership
// of the passed in pointer.
void ChromeNotifierService::Add(scoped_ptr<SyncedNotification> notification) {
  SyncedNotification* notification_copy = notification.get();
  // Take ownership of the object and put it into our local storage.
  notification_data_.push_back(notification.release());

  // Show it to the user.
  Show(notification_copy);
}

// Send the notification to the NotificationUIManager to show to the user.
void ChromeNotifierService::Show(SyncedNotification* notification) {
  // Set up the fields we need to send and create a Notification object.
  GURL origin_url(notification->GetOriginUrl());
  GURL app_icon_url(notification->GetAppIconUrl());
  GURL image_url(notification->GetImageUrl());
  string16 title = UTF8ToUTF16(notification->GetTitle());
  string16 text = UTF8ToUTF16(notification->GetText());
  string16 heading = UTF8ToUTF16(notification->GetHeading());
  string16 description = UTF8ToUTF16(notification->GetDescription());
  // TODO(petewil): What goes in the display source, is empty OK?
  string16 display_source;
  string16 replace_key = UTF8ToUTF16(notification->GetKey());

  // The delegate will eventually catch calls that the notification
  // was read or deleted, and send the changes back to the server.
  scoped_refptr<NotificationDelegate> delegate =
      new ChromeNotifierDelegate(notification->GetKey(), this);

  // Some inputs and fields are only used if there is a notification center.
#if defined(ENABLE_MESSAGE_CENTER)
  double creation_time = static_cast<double>(notification->GetCreationTime());
  int priority = notification->GetPriority();
  int notification_count = notification->GetNotificationCount();
  int button_count = notification->GetButtonCount();
  std::string button_one_title = notification->GetButtonOneTitle();
  std::string button_one_icon_url = notification->GetButtonOneIconUrl();
  std::string button_two_title = notification->GetButtonTwoTitle();
  std::string button_two_icon_url = notification->GetButtonTwoIconUrl();

  // Deduce which notification template to use from the data.
  message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  if (!image_url.is_empty()) {
    notification_type = message_center::NOTIFICATION_TYPE_IMAGE;
  } else if (notification_count > 1) {
    notification_type = message_center::NOTIFICATION_TYPE_MULTIPLE;
  } else if (button_count > 0) {
    notification_type = message_center::NOTIFICATION_TYPE_BASE_FORMAT;
  }

  // Fill the optional fields with the information we need to make a
  // notification.
  DictionaryValue optional_fields;
  optional_fields.SetDouble(message_center::kTimestampKey, creation_time);
  if (priority != SyncedNotification::kUndefinedPriority)
    optional_fields.SetInteger(message_center::kPriorityKey, priority);
  if (!button_one_title.empty())
    optional_fields.SetString(message_center::kButtonOneTitleKey,
                               button_one_title);
  if (!button_one_icon_url.empty())
    optional_fields.SetString(message_center::kButtonOneIconUrlKey,
                               button_one_icon_url);
  if (!button_two_title.empty())
    optional_fields.SetString(message_center::kButtonTwoTitleKey,
                               button_two_title);
  if (!button_two_icon_url.empty())
    optional_fields.SetString(message_center::kButtonTwoIconUrlKey,
                               button_two_icon_url);

  // Fill the individual notification fields for a multiple notification.
  if (notification_count > 1) {
    base::ListValue* items = new base::ListValue();

    for (int ii = 0; ii < notification_count; ++ii) {
      DictionaryValue* item = new DictionaryValue();
      item->SetString(message_center::kItemTitleKey,
                      UTF8ToUTF16(notification->GetContainedNotificationTitle(
                          ii)));
      item->SetString(message_center::kItemMessageKey,
                      UTF8ToUTF16(notification->GetContainedNotificationMessage(
                          ii)));
      items->Append(item);
    }

    optional_fields.Set(message_center::kItemsKey, items);
  }

  // TODO(petewil): For now, just punt on dismissed notifications until
  // I change the interface to let NotificationUIManager know the right way.
  if (SyncedNotification::kRead == notification->GetReadState() ||
      SyncedNotification::kDismissed == notification->GetReadState() ) {
    DVLOG(2) << "Dismissed notification arrived"
             << notification->GetTitle() << " " << notification->GetText();
    return;
  }

  Notification ui_notification(notification_type,
                               origin_url,
                               app_icon_url,
                               heading,
                               text,
                               WebKit::WebTextDirectionDefault,
                               display_source,
                               replace_key,
                               &optional_fields,
                               delegate);


#else  // ENABLE_MESSAGE_CENTER

  Notification ui_notification(origin_url,
                               app_icon_url,
                               heading,
                               text,
                               WebKit::WebTextDirectionDefault,
                               display_source,
                               replace_key,
                               delegate);

#endif  // ENABLE_MESSAGE_CENTER

  notification_manager_->Add(ui_notification, profile_);

  DVLOG(1) << "Synced Notification arrived! " << title << " " << text
           << " " << app_icon_url << " " << replace_key << " "
           << notification->GetReadState();

  return;
}

}  // namespace notifier
