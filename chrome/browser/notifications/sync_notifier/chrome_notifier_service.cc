// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ChromeNotifierService works together with sync to maintain the state of
// user notifications, which can then be presented in the notification center,
// via the Notification UI Manager.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/welcome_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

using base::UserMetricsAction;

namespace notifier {
const char kFirstSyncedNotificationServiceId[] = "Google+";
const char kSyncedNotificationsWelcomeOrigin[] =
    "synced-notifications://welcome";

bool ChromeNotifierService::avoid_bitmap_fetching_for_test_ = false;

ChromeNotifierService::ChromeNotifierService(Profile* profile,
                                             NotificationUIManager* manager)
    : profile_(profile),
      notification_manager_(manager),
      synced_notification_first_run_(false) {

  InitializePrefs();

  // Get a pointer to the app info service so we can get app info data.
  synced_notification_app_info_service_ =
      SyncedNotificationAppInfoServiceFactory::GetForProfile(
          profile_, Profile::IMPLICIT_ACCESS);

  DCHECK(synced_notification_app_info_service_ != NULL);

  synced_notification_app_info_service_->set_chrome_notifier_service(this);

}

ChromeNotifierService::~ChromeNotifierService() {
  if (synced_notification_app_info_service_)
    synced_notification_app_info_service_->set_chrome_notifier_service(NULL);
}

// Methods from KeyedService.
void ChromeNotifierService::Shutdown() {}

// syncer::SyncableService implementation.

// This is called at startup to sync with the server.
// This code is not thread safe.
syncer::SyncMergeResult ChromeNotifierService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  thread_checker_.CalledOnValidThread();
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
    if (!incoming) {
      NOTREACHED();
      continue;
    }

    // Process each incoming remote notification.
    const std::string& key = incoming->GetKey();
    DCHECK_GT(key.length(), 0U);
    SyncedNotification* found = FindNotificationById(key);

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
        } else {
          // If the read state is different, read wins for both places.
          if (incoming->GetReadState() == SyncedNotification::kDismissed) {
            // If it is marked as read on the server, but not the client.
            found->NotificationHasBeenDismissed();
            // Tell the Notification UI Manager to remove it.
            notification_manager_->CancelById(found->GetKey());
          } else if (incoming->GetReadState() == SyncedNotification::kRead) {
            // If it is marked as read on the server, but not the client.
            found->NotificationHasBeenRead();
            // Tell the Notification UI Manager to remove it.
            notification_manager_->CancelById(found->GetKey());
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
      } else {
        // If different, just replace the local with the remote.
        // TODO(petewil): Someday we may allow changes from the client to
        // flow upwards, when we do, we will need better merge resolution.
        found->Update(sync_data);

        // Tell the notification manager to update the notification.
        UpdateInMessageCenter(found);
      }
    }
  }

  // Send up the changes that were made locally.
  if (new_changes.size() > 0) {
    merge_result.set_error(sync_processor_->ProcessSyncChanges(
        FROM_HERE, new_changes));
  }

  // Once we complete our first sync, we mark "first run" as false,
  // subsequent runs of Synced Notifications will get normal treatment.
  if (synced_notification_first_run_) {
    synced_notification_first_run_ = false;
    profile_->GetPrefs()->SetBoolean(prefs::kSyncedNotificationFirstRun, false);
  }

  return merge_result;
}

void ChromeNotifierService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, type);
  // Since this data type is not user-unselectable, we chose not to implement
  // the stop syncing method, and instead do nothing here.
}

syncer::SyncDataList ChromeNotifierService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATIONS, type);
  syncer::SyncDataList sync_data;

  // Copy our native format data into a SyncDataList format.
  ScopedVector<SyncedNotification>::const_iterator it =
      notification_data_.begin();
  for (; it != notification_data_.end(); ++it) {
    sync_data.push_back(CreateSyncDataFromNotification(**it));
  }

  return sync_data;
}

// This method is called when there is an incoming sync change from the server.
syncer::SyncError ChromeNotifierService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  thread_checker_.CalledOnValidThread();
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

    const std::string& key = new_notification->GetKey();
    DCHECK_GT(key.length(), 0U);
    SyncedNotification* found = FindNotificationById(key);

    switch (change_type) {
      case syncer::SyncChange::ACTION_ADD:
        // Intentional fall through, cases are identical.
      case syncer::SyncChange::ACTION_UPDATE:
        if (found == NULL) {
          Add(new_notification.Pass());
          break;
        }
        // Update it in our store.
        found->Update(sync_data);
        // Tell the notification manager to update the notification.
        UpdateInMessageCenter(found);
        break;

      case syncer::SyncChange::ACTION_DELETE:
        if (found == NULL) {
          break;
        }
        // Remove it from our store.
        FreeNotificationById(key);
        // Remove it from the message center.
        UpdateInMessageCenter(new_notification.get());
        // TODO(petewil): Do I need to remember that it was deleted in case the
        // add arrives after the delete?  If so, how long do I need to remember?
        break;

      default:
        NOTREACHED();
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

// Convert from SyncData to our internal format.
scoped_ptr<SyncedNotification>
ChromeNotifierService::CreateNotificationFromSyncData(
    const syncer::SyncData& sync_data) {
  // Get a pointer to our data within the sync_data object.
  sync_pb::SyncedNotificationSpecifics specifics =
      sync_data.GetSpecifics().synced_notification();

  // Check for mandatory fields in the sync_data object.
  if (!specifics.has_coalesced_notification() ||
      !specifics.coalesced_notification().has_key() ||
      !specifics.coalesced_notification().has_read_state()) {
    DVLOG(1) << "Synced Notification missing mandatory fields "
             << "has coalesced notification? "
             << specifics.has_coalesced_notification()
             << " has key? " << specifics.coalesced_notification().has_key()
             << " has read state? "
             << specifics.coalesced_notification().has_read_state();
    return scoped_ptr<SyncedNotification>();
  }

  bool is_well_formed_unread_notification =
      (static_cast<SyncedNotification::ReadState>(
          specifics.coalesced_notification().read_state()) ==
       SyncedNotification::kUnread &&
       specifics.coalesced_notification().has_render_info());
  bool is_well_formed_read_notification =
      (static_cast<SyncedNotification::ReadState>(
          specifics.coalesced_notification().read_state()) ==
       SyncedNotification::kRead);
  bool is_well_formed_dismissed_notification =
      (static_cast<SyncedNotification::ReadState>(
          specifics.coalesced_notification().read_state()) ==
       SyncedNotification::kDismissed);

  // If the notification is poorly formed, return a null pointer.
  if (!is_well_formed_unread_notification &&
      !is_well_formed_read_notification &&
      !is_well_formed_dismissed_notification) {
    DVLOG(1) << "Synced Notification is not well formed."
             << " unread well formed? "
             << is_well_formed_unread_notification
             << " dismissed well formed? "
             << is_well_formed_dismissed_notification
             << " read well formed? "
             << is_well_formed_read_notification;
    return scoped_ptr<SyncedNotification>();
  }

  // Create a new notification object based on the supplied sync_data.
  scoped_ptr<SyncedNotification> notification(
      new SyncedNotification(sync_data, this, notification_manager_));

  return notification.Pass();
}

// This returns a pointer into a vector that we own.  Caller must not free it.
// Returns NULL if no match is found.
SyncedNotification* ChromeNotifierService::FindNotificationById(
    const std::string& notification_id) {
  // TODO(petewil): We can make a performance trade off here.
  // While the vector has good locality of reference, a map has faster lookup.
  // Based on how big we expect this to get, maybe change this to a map.
  ScopedVector<SyncedNotification>::const_iterator it =
      notification_data_.begin();
  for (; it != notification_data_.end(); ++it) {
    SyncedNotification* notification = *it;
    if (notification_id == notification->GetKey())
      return *it;
  }

  return NULL;
}

void ChromeNotifierService::FreeNotificationById(
    const std::string& notification_id) {
  ScopedVector<SyncedNotification>::iterator it = notification_data_.begin();
  for (; it != notification_data_.end(); ++it) {
    SyncedNotification* notification = *it;
    if (notification_id == notification->GetKey()) {
      notification_data_.erase(it);
      return;
    }
  }
}

void ChromeNotifierService::GetSyncedNotificationServices(
    std::vector<message_center::Notifier*>* notifiers) {
  // TODO(mukai|petewil): Check the profile's eligibility before adding the
  // sample app.
  std::vector<SyncedNotificationSendingServiceSettingsData>
      sending_service_settings_data;

  if (synced_notification_app_info_service_ == NULL)
    return;

  sending_service_settings_data =
      synced_notification_app_info_service_->GetAllSendingServiceSettingsData();

  for (size_t ii = 0; ii < sending_service_settings_data.size(); ++ii) {

    // Enable or disable the sending service per saved preferences.
    bool app_enabled = false;
    std::set<std::string>::iterator iter;
    iter = find(enabled_sending_services_.begin(),
                enabled_sending_services_.end(),
                sending_service_settings_data[ii].notifier_id.id);
    app_enabled = iter != enabled_sending_services_.end();

    message_center::Notifier* app_info_notifier = new message_center::Notifier(
        sending_service_settings_data[ii].notifier_id,
        base::UTF8ToUTF16(
            sending_service_settings_data[ii].settings_display_name),
        app_enabled);

    app_info_notifier->icon = sending_service_settings_data[ii].settings_icon;

    // |notifiers| takes ownership of |app_info_notifier|.
    notifiers->push_back(app_info_notifier);
  }
}

void ChromeNotifierService::OnAddedAppIds(
    std::vector<std::string> added_app_ids) {

  std::vector<std::string>::const_iterator app_id_iter;
  for (app_id_iter = added_app_ids.begin(); app_id_iter != added_app_ids.end();
       ++app_id_iter) {
    // Make sure this is not a dup, if it is, do nothing.
    // TODO(petewil): consider a case insensitive compare.
    std::set<std::string>::iterator sending_service_iter;
    sending_service_iter = enabled_sending_services_.find(*app_id_iter);
    if (sending_service_iter != enabled_sending_services_.end())
      continue;

    // Find any newly enabled notifications and call display on them.
    // Show the welcome toast if required.
    ScopedVector<SyncedNotification>::iterator notification_iter;
    for (notification_iter = notification_data_.begin();
         notification_iter != notification_data_.end();
         ++notification_iter) {
      (*notification_iter)->ShowAllForAppId(profile_, *app_id_iter);
    }
  }
}

void ChromeNotifierService::OnRemovedAppIds(
    std::vector<std::string> removed_app_ids) {
  // Remove from enabled sending services.
  // Don't remove from initialized sending services. If it gets re-added later,
  // we want to remember the user's decision, so we also leave prefs intact.

  // Find any displayed notifications and remove them from the notification
  // center.
  std::vector<std::string>::const_iterator app_id_iter;
  for (app_id_iter = removed_app_ids.begin();
       app_id_iter != removed_app_ids.end();
       ++app_id_iter) {
    // Find any newly disabled notifications and remove them.
    ScopedVector<SyncedNotification>::iterator notification_iter;
    for (notification_iter = notification_data_.begin();
         notification_iter != notification_data_.end();
         ++notification_iter) {
      (*notification_iter)->HideAllForAppId(*app_id_iter);
    }
  }
}

void ChromeNotifierService::MarkNotificationAsRead(
    const std::string& key) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SyncedNotification* notification = FindNotificationById(key);
  CHECK(notification != NULL);

  notification->NotificationHasBeenRead();
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

  // If the user is not interested in this type of notification, ignore it.
  std::string sending_service_id = GetSendingServiceId(notification_copy);
  std::set<std::string>::iterator iter = find(enabled_sending_services_.begin(),
                                              enabled_sending_services_.end(),
                                              sending_service_id);
  if (iter == enabled_sending_services_.end()) {
    iter = find(initialized_sending_services_.begin(),
                initialized_sending_services_.end(),
                sending_service_id);
    if (iter != initialized_sending_services_.end())
      return;
  }

  UpdateInMessageCenter(notification_copy);
}

std::string ChromeNotifierService::GetSendingServiceId(
    const SyncedNotification* synced_notification) {
  // Get the App Id from the notification, and look it up in the synced
  // notification app info service.
  std::string app_id = synced_notification->GetAppId();

  DCHECK(synced_notification_app_info_service_ != NULL);

  return synced_notification_app_info_service_->FindSendingServiceNameFromAppId(
      app_id);
}

void ChromeNotifierService::AddForTest(
    scoped_ptr<notifier::SyncedNotification> notification) {
  notification_data_.push_back(notification.release());
}

void ChromeNotifierService::SetSyncedNotificationAppInfoServiceForTest(
    SyncedNotificationAppInfoService* synced_notification_app_info_service) {
  // If there already is a service attached, clear their reference to us.
  if (synced_notification_app_info_service_)
    synced_notification_app_info_service_->set_chrome_notifier_service(NULL);

  synced_notification_app_info_service_ = synced_notification_app_info_service;
  synced_notification_app_info_service_->set_chrome_notifier_service(this);
}

void ChromeNotifierService::UpdateInMessageCenter(
    SyncedNotification* notification) {
  // If the feature is disabled, exit now.
  if (!notifier::ChromeNotifierServiceFactory::UseSyncedNotifications(
      CommandLine::ForCurrentProcess()))
    return;

  notification->LogNotification();

  if (notification->GetReadState() == SyncedNotification::kUnread) {
    // If the message is unread, update it.
    Display(notification);
  } else {
    // If the message is read or deleted, dismiss it from the center.
    // We intentionally ignore errors if it is not in the center.
    notification_manager_->CancelById(notification->GetKey());
  }
}

void ChromeNotifierService::Display(SyncedNotification* notification) {
  // If this is the first run for the feature, don't surprise the user.
  // Instead, place all backlogged notifications into the notification
  // center.
  if (synced_notification_first_run_) {
    // Setting the toast state to false will prevent toasting the notification.
    notification->set_toast_state(false);
  }

  // Our tests cannot use the network for reliability reasons.
  if (avoid_bitmap_fetching_for_test_) {
    notification->Show(profile_);
    return;
  }

  // Set up to fetch the bitmaps.
  notification->QueueBitmapFetchJobs(this, profile_);

  // Start the bitmap fetching, Show() will be called when the last bitmap
  // either arrives or times out.
  notification->StartBitmapFetch();
}

void ChromeNotifierService::OnSyncedNotificationServiceEnabled(
    const std::string& notifier_id, bool enabled) {
  std::set<std::string>::iterator iter;

  // Make a copy of the notifier_id, which might not have lifetime long enough
  // for this function to finish all of its work.
  std::string notifier_id_copy(notifier_id);

  iter = find(enabled_sending_services_.begin(),
              enabled_sending_services_.end(),
              notifier_id_copy);

  base::ListValue synced_notification_services;

  // Add the notifier_id if it is enabled and not already there.
  if (iter == enabled_sending_services_.end() && enabled) {
    enabled_sending_services_.insert(notifier_id_copy);
    // Check now for any outstanding notifications.
    DisplayUnreadNotificationsFromSource(notifier_id);
    BuildServiceListValueInplace(enabled_sending_services_,
                                 &synced_notification_services);
    // Add this preference to the enabled list.
    profile_->GetPrefs()->Set(prefs::kEnabledSyncedNotificationSendingServices,
                              synced_notification_services);
    // Remove the notifier_id if it is disabled and present.
  } else if (iter != enabled_sending_services_.end() && !enabled) {
    enabled_sending_services_.erase(iter);
    BuildServiceListValueInplace(enabled_sending_services_,
                                 &synced_notification_services);
    // Remove this peference from the enabled list.
    profile_->GetPrefs()->Set(prefs::kEnabledSyncedNotificationSendingServices,
                              synced_notification_services);
    RemoveUnreadNotificationsFromSource(notifier_id_copy);
  }

  // Collect UMA statistics when a service is enabled or disabled.
  if (enabled) {
    content::RecordAction(
        UserMetricsAction("SyncedNotifications.SendingServiceEnabled"));
  } else {
    content::RecordAction(
        UserMetricsAction("SyncedNotifications.SendingServiceDisabled"));
  }

  // Collect individual service enabling/disabling statistics.
  CollectPerServiceEnablingStatistics(notifier_id, enabled);

  return;
}

void ChromeNotifierService::CollectPerServiceEnablingStatistics(
    const std::string& notifier_id,
    bool enabled) {
  // TODO(petewil) - This approach does not scale well as we add new services,
  // but we are limited to using predefined ENUM values in histogram based UMA
  // data, which does not permit arbitrary strings.
  // Find a way to make it scale, or remove enum value this when we have enough
  // data.

  ChromeNotifierServiceActionType action =
      CHROME_NOTIFIER_SERVICE_ACTION_UNKNOWN;

  // Derive action type from notifier_id and enabled.
  // TODO(petewil): Add more sending services as they are enabled.
  if (notifier_id == std::string(kFirstSyncedNotificationServiceId)) {
    action = enabled
        ? CHROME_NOTIFIER_SERVICE_ACTION_FIRST_SERVICE_ENABLED
        : CHROME_NOTIFIER_SERVICE_ACTION_FIRST_SERVICE_DISABLED;
  }

  UMA_HISTOGRAM_ENUMERATION("ChromeNotifierService.Actions",
                            action,
                            CHROME_NOTIFIER_SERVICE_ACTION_COUNT);
}

void ChromeNotifierService::BuildServiceListValueInplace(
    std::set<std::string> services, base::ListValue* list_value) {
  std::set<std::string>::iterator iter;

  // Iterate over the strings, adding each one to the list value
  for (iter = services.begin();
       iter != services.end();
       ++iter) {
    base::StringValue* string_value(new base::StringValue(*iter));
    list_value->Append(string_value);
  }
}

void ChromeNotifierService::DisplayUnreadNotificationsFromSource(
    const std::string& notifier_id) {
  for (std::vector<SyncedNotification*>::const_iterator iter =
          notification_data_.begin();
       iter != notification_data_.end();
       ++iter) {
    if (GetSendingServiceId(*iter) == notifier_id &&
        (*iter)->GetReadState() == SyncedNotification::kUnread)
      Display(*iter);
  }
}

void ChromeNotifierService::RemoveUnreadNotificationsFromSource(
    const std::string& notifier_id) {
  for (std::vector<SyncedNotification*>::const_iterator iter =
          notification_data_.begin();
       iter != notification_data_.end();
       ++iter) {
    if (GetSendingServiceId(*iter) == notifier_id &&
        (*iter)->GetReadState() == SyncedNotification::kUnread) {
      notification_manager_->CancelById((*iter)->GetKey());
    }
  }
}

void ChromeNotifierService::OnEnabledSendingServiceListPrefChanged(
    std::set<std::string>* ids_field) {
  ids_field->clear();
  const std::vector<std::string> pref_list =
      enabled_sending_services_prefs_.GetValue();
  for (size_t i = 0; i < pref_list.size(); ++i) {
    std::string element = pref_list[i];
    if (!element.empty())
      ids_field->insert(element);
    else
      LOG(WARNING) << i << "-th element is not a string "
                   << prefs::kEnabledSyncedNotificationSendingServices;
  }
}

void ChromeNotifierService::OnInitializedSendingServiceListPrefChanged(
    std::set<std::string>* ids_field) {
  ids_field->clear();
  const std::vector<std::string> pref_list =
      initialized_sending_services_prefs_.GetValue();
  for (size_t i = 0; i < pref_list.size(); ++i) {
    std::string element = pref_list[i];
    if (!element.empty())
      ids_field->insert(element);
    else
      LOG(WARNING) << i << "-th element is not a string for "
                   << prefs::kInitializedSyncedNotificationSendingServices;
  }
}

void ChromeNotifierService::OnSyncedNotificationFirstRunBooleanPrefChanged(
    bool* new_value) {
  synced_notification_first_run_ = *new_value;
}

void ChromeNotifierService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Register the pref for the list of enabled services.
  registry->RegisterListPref(
      prefs::kEnabledSyncedNotificationSendingServices,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // Register the pref for the list of initialized services.
  registry->RegisterListPref(
      prefs::kInitializedSyncedNotificationSendingServices,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // Register the preference for first run status, defaults to "true",
  // meaning that this is the first run of the Synced Notification feature.
  registry->RegisterBooleanPref(
      prefs::kSyncedNotificationFirstRun, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void ChromeNotifierService::InitializePrefs() {
  // Set up any pref changes to update our list of services.
  enabled_sending_services_prefs_.Init(
      prefs::kEnabledSyncedNotificationSendingServices,
      profile_->GetPrefs(),
      base::Bind(
          &ChromeNotifierService::OnEnabledSendingServiceListPrefChanged,
          base::Unretained(this),
          base::Unretained(&enabled_sending_services_)));
  initialized_sending_services_prefs_.Init(
      prefs::kInitializedSyncedNotificationSendingServices,
      profile_->GetPrefs(),
      base::Bind(
          &ChromeNotifierService::OnInitializedSendingServiceListPrefChanged,
          base::Unretained(this),
          base::Unretained(&initialized_sending_services_)));
  synced_notification_first_run_prefs_.Init(
      prefs::kSyncedNotificationFirstRun,
      profile_->GetPrefs(),
      base::Bind(
          &ChromeNotifierService::
              OnSyncedNotificationFirstRunBooleanPrefChanged,
          base::Unretained(this),
          base::Unretained(&synced_notification_first_run_)));

  // Get the prefs from last session into our memeber varilables
  OnEnabledSendingServiceListPrefChanged(&enabled_sending_services_);
  OnInitializedSendingServiceListPrefChanged(&initialized_sending_services_);

  synced_notification_first_run_ =
      profile_->GetPrefs()->GetBoolean(prefs::kSyncedNotificationFirstRun);
}

void ChromeNotifierService::ShowWelcomeToastIfNecessary(
    const SyncedNotification* synced_notification,
    NotificationUIManager* notification_ui_manager) {
  const std::string& sending_service_id =
      GetSendingServiceId(synced_notification);

  std::set<std::string>::iterator iter;
  iter = find(initialized_sending_services_.begin(),
              initialized_sending_services_.end(),
              sending_service_id);

  // If we already initialized the sending service, then return early since no
  // welcome toast is necessary.
  if (iter != initialized_sending_services_.end())
    return;

  // If there is no app info, we can't show a welcome toast.  All synced
  // notifications will be delayed until an app_info data structure can be
  // constructed for them.
  notifier::SyncedNotificationAppInfo* app_info =
      FindAppInfoByAppId(synced_notification->GetAppId());
  if (!app_info)
    return;

  if (app_info->settings_icon_url().is_empty()) {
    gfx::Image notification_app_icon = synced_notification->GetAppIcon();
    if (notification_app_icon.IsEmpty()) {
      // This block should only be reached in tests since the downloads are
      // already finished for |synced_notification|.
      DVLOG(1) << "Unable to find the app icon for the welcome notification. "
               << "Service ID: " << sending_service_id;
    }
  }

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      sending_service_id);

  Notification notification = CreateWelcomeNotificationForService(app_info);
  notification_ui_manager->Add(notification, profile_);

  enabled_sending_services_.insert(sending_service_id);
  initialized_sending_services_.insert(sending_service_id);

  // Build a ListValue with the list of services to be enabled.
  base::ListValue enabled_sending_services;
  base::ListValue initialized_sending_services;

  // Mark any new services as enabled in preferences.
  BuildServiceListValueInplace(enabled_sending_services_,
                               &enabled_sending_services);
  profile_->GetPrefs()->Set(prefs::kEnabledSyncedNotificationSendingServices,
                            enabled_sending_services);
  // Mark any new services as initialized in preferences.
  BuildServiceListValueInplace(initialized_sending_services_,
                               &initialized_sending_services);
  profile_->GetPrefs()->Set(
      prefs::kInitializedSyncedNotificationSendingServices,
      initialized_sending_services);
}

notifier::SyncedNotificationAppInfo* ChromeNotifierService::FindAppInfoByAppId(
    const std::string& app_id) const {
  if (NULL == synced_notification_app_info_service_)
    return NULL;

  return synced_notification_app_info_service_->
      FindSyncedNotificationAppInfoByAppId(app_id);
}

const Notification ChromeNotifierService::CreateWelcomeNotificationForService(
    SyncedNotificationAppInfo* app_info) {
  std::string welcome_notification_id = base::GenerateGUID();
  message_center::NotifierId notifier_id = app_info->GetNotifierId();
  scoped_refptr<WelcomeDelegate> delegate(
      new WelcomeDelegate(welcome_notification_id, profile_, notifier_id));

  message_center::ButtonInfo button_info(
      l10n_util::GetStringUTF16(IDS_NOTIFIER_WELCOME_BUTTON));
  button_info.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_NOTIFIER_BLOCK_BUTTON);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(button_info);
  return Notification(
      message_center::NOTIFICATION_TYPE_BASE_FORMAT,
      GURL(kSyncedNotificationsWelcomeOrigin),
      base::UTF8ToUTF16(app_info->settings_display_name()),
      l10n_util::GetStringUTF16(IDS_NOTIFIER_WELCOME_BODY),
      app_info->icon(),
      blink::WebTextDirectionDefault,
      notifier_id,
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_WELCOME_DISPLAY_SOURCE),
      base::UTF8ToUTF16(welcome_notification_id),
      rich_notification_data,
      delegate.get());
}

}  // namespace notifier
