// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autocomplete_syncable_service.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace {

const char kAutofillEntryNamespaceTag[] = "autofill_entry|";

// Merges timestamps from the |autofill| entry and |timestamps|. Returns
// true if they were different, false if they were the same.
// All of the timestamp vectors are assummed to be sorted, resulting vector is
// sorted as well.
bool MergeTimestamps(const sync_pb::AutofillSpecifics& autofill,
                     const std::vector<base::Time>& timestamps,
                     std::vector<base::Time>* new_timestamps) {
  DCHECK(new_timestamps);
  std::set<base::Time> timestamp_union(timestamps.begin(),
                                       timestamps.end());

  size_t timestamps_count = autofill.usage_timestamp_size();

  bool different = timestamps.size() != timestamps_count;
  for (size_t i = 0; i < timestamps_count; ++i) {
    if (timestamp_union.insert(base::Time::FromInternalValue(
            autofill.usage_timestamp(i))).second) {
      different = true;
    }
  }

  if (different) {
    new_timestamps->insert(new_timestamps->begin(),
                           timestamp_union.begin(),
                           timestamp_union.end());
  }
  return different;
}

}  // namespace

AutocompleteSyncableService::AutocompleteSyncableService(
    WebDataService* web_data_service)
    : web_data_service_(web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(web_data_service_);
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
      content::Source<WebDataService>(web_data_service));
}

AutocompleteSyncableService::~AutocompleteSyncableService() {
  DCHECK(CalledOnValidThread());
}

AutocompleteSyncableService::AutocompleteSyncableService()
    : web_data_service_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
}

SyncError AutocompleteSyncableService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  DCHECK(CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  VLOG(1) << "Associating Autocomplete: MergeDataAndStartSyncing";

  std::vector<AutofillEntry> entries;
  if (!LoadAutofillData(&entries)) {
    return SyncError(
        FROM_HERE, "Could not get the autocomplete data from WebDatabase.",
        model_type());
  }

  AutocompleteEntryMap new_db_entries;
  for (std::vector<AutofillEntry>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    new_db_entries[it->key()] = std::make_pair(SyncChange::ACTION_ADD, it);
  }

  sync_processor_.reset(sync_processor);

  std::vector<AutofillEntry> new_synced_entries;
  // Go through and check for all the entries that sync already knows about.
  // CreateOrUpdateEntry() will remove entries that are same with the synced
  // ones from |new_db_entries|.
  for (SyncDataList::const_iterator sync_iter = initial_sync_data.begin();
       sync_iter != initial_sync_data.end(); ++sync_iter) {
    CreateOrUpdateEntry(*sync_iter, &new_db_entries, &new_synced_entries);
  }

  if (!SaveChangesToWebData(new_synced_entries))
    return SyncError(FROM_HERE, "Failed to update webdata.", model_type());

  WebDataService::NotifyOfMultipleAutofillChanges(web_data_service_);

  SyncChangeList new_changes;
  for (AutocompleteEntryMap::iterator i = new_db_entries.begin();
       i != new_db_entries.end(); ++i) {
    new_changes.push_back(
        SyncChange(i->second.first, CreateSyncData(*(i->second.second))));
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);

  return error;
}

void AutocompleteSyncableService::StopSyncing(syncable::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(syncable::AUTOFILL, type);

  sync_processor_.reset(NULL);
}

SyncDataList AutocompleteSyncableService::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_.get());
  DCHECK_EQ(type, syncable::AUTOFILL);

  SyncDataList current_data;

  std::vector<AutofillEntry> entries;
  if (!LoadAutofillData(&entries))
    return current_data;

  for (std::vector<AutofillEntry>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    current_data.push_back(CreateSyncData(*it));
  }

  return current_data;
}

SyncError AutocompleteSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_.get());

  if (!sync_processor_.get()) {
    SyncError error(FROM_HERE, "Models not yet associated.",
                    syncable::AUTOFILL);
    return error;
  }

  // Data is loaded only if we get new ADD/UPDATE change.
  std::vector<AutofillEntry> entries;
  scoped_ptr<AutocompleteEntryMap> db_entries;
  std::vector<AutofillEntry> new_entries;

  SyncError list_processing_error;

  for (SyncChangeList::const_iterator i = change_list.begin();
       i != change_list.end() && !list_processing_error.IsSet(); ++i) {
    DCHECK(i->IsValid());
    switch (i->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE:
        if (!db_entries.get()) {
          if (!LoadAutofillData(&entries)) {
            return SyncError(
                FROM_HERE,
                "Could not get the autocomplete data from WebDatabase.",
                model_type());
          }
          db_entries.reset(new AutocompleteEntryMap);
          for (std::vector<AutofillEntry>::iterator it = entries.begin();
               it != entries.end(); ++it) {
            (*db_entries)[it->key()] =
                std::make_pair(SyncChange::ACTION_ADD, it);
          }
        }
        CreateOrUpdateEntry(i->sync_data(), db_entries.get(), &new_entries);
        break;
      case SyncChange::ACTION_DELETE: {
        DCHECK(i->sync_data().GetSpecifics().HasExtension(sync_pb::autofill))
            << "Autofill specifics data not present on delete!";
        const sync_pb::AutofillSpecifics& autofill =
            i->sync_data().GetSpecifics().GetExtension(sync_pb::autofill);
        if (autofill.has_value()) {
          list_processing_error = AutofillEntryDelete(autofill);
        } else {
          DLOG(WARNING)
              << "Delete for old-style autofill profile being dropped!";
        }
      } break;
      default:
        NOTREACHED() << "Unexpected sync change state.";
        return SyncError(FROM_HERE, "ProcessSyncChanges failed on ChangeType " +
                         SyncChange::ChangeTypeToString(i->change_type()),
                         syncable::AUTOFILL);
    }
  }

  if (!SaveChangesToWebData(new_entries))
    return SyncError(FROM_HERE, "Failed to update webdata.", model_type());

  WebDataService::NotifyOfMultipleAutofillChanges(web_data_service_);

  return list_processing_error;
}

void AutocompleteSyncableService::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED, type);

  // Check if sync is on. If we receive notification prior to the sync being set
  // up we are going to process all when MergeData..() is called. If we receive
  // notification after the sync exited, it will be sinced next time Chrome
  // starts.
  if (!sync_processor_.get())
    return;
  WebDataService* wds = content::Source<WebDataService>(source).ptr();

  DCHECK_EQ(web_data_service_, wds);

  AutofillChangeList* changes =
      content::Details<AutofillChangeList>(details).ptr();
  ActOnChanges(*changes);
}

bool AutocompleteSyncableService::LoadAutofillData(
    std::vector<AutofillEntry>* entries) const {
  return web_data_service_->GetDatabase()->
             GetAutofillTable()->GetAllAutofillEntries(entries);
}

bool AutocompleteSyncableService::SaveChangesToWebData(
    const std::vector<AutofillEntry>& new_entries) {
  DCHECK(CalledOnValidThread());

  if (!new_entries.empty() &&
      !web_data_service_->GetDatabase()->
          GetAutofillTable()->UpdateAutofillEntries(new_entries)) {
    return false;
  }
  return true;
}

// Creates or updates an autocomplete entry based on |data|.
void AutocompleteSyncableService::CreateOrUpdateEntry(
    const SyncData& data,
    AutocompleteEntryMap* loaded_data,
    std::vector<AutofillEntry>* new_entries) {
  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::AutofillSpecifics& autofill_specifics(
      specifics.GetExtension(sync_pb::autofill));

  if (!autofill_specifics.has_value()) {
    DLOG(WARNING)
        << "Add/Update for old-style autofill profile being dropped!";
    return;
  }

  AutofillKey key(autofill_specifics.name().c_str(),
                  autofill_specifics.value().c_str());
  AutocompleteEntryMap::iterator it = loaded_data->find(key);
  if (it == loaded_data->end()) {
    // New entry.
    std::vector<base::Time> timestamps;
    size_t timestamps_count = autofill_specifics.usage_timestamp_size();
    timestamps.resize(timestamps_count);
    for (size_t ts = 0; ts < timestamps_count; ++ts) {
      timestamps[ts] = base::Time::FromInternalValue(
            autofill_specifics.usage_timestamp(ts));
    }
    new_entries->push_back(AutofillEntry(key, timestamps));
  } else {
    // Entry already present - merge if necessary.
    std::vector<base::Time> timestamps;
    bool different = MergeTimestamps(
        autofill_specifics, it->second.second->timestamps(), &timestamps);
    if (different) {
      AutofillEntry new_entry(it->second.second->key(), timestamps);
      new_entries->push_back(new_entry);

      // Update the sync db if the list of timestamps have changed.
      *(it->second.second) = new_entry;
      it->second.first = SyncChange::ACTION_UPDATE;
    } else {
      loaded_data->erase(it);
    }
  }
}

// static
void AutocompleteSyncableService::WriteAutofillEntry(
    const AutofillEntry& entry, sync_pb::EntitySpecifics* autofill_specifics) {
  sync_pb::AutofillSpecifics* autofill =
      autofill_specifics->MutableExtension(sync_pb::autofill);
  autofill->set_name(UTF16ToUTF8(entry.key().name()));
  autofill->set_value(UTF16ToUTF8(entry.key().value()));
  const std::vector<base::Time>& ts(entry.timestamps());
  for (std::vector<base::Time>::const_iterator timestamp = ts.begin();
       timestamp != ts.end(); ++timestamp) {
    autofill->add_usage_timestamp(timestamp->ToInternalValue());
  }
}

SyncError AutocompleteSyncableService::AutofillEntryDelete(
    const sync_pb::AutofillSpecifics& autofill) {
  if (!web_data_service_->GetDatabase()->GetAutofillTable()->RemoveFormElement(
          UTF8ToUTF16(autofill.name()), UTF8ToUTF16(autofill.value()))) {
    return SyncError(FROM_HERE,
                     "Could not remove autocomplete entry from WebDatabase.",
                     model_type());
  }
  return SyncError();
}

void AutocompleteSyncableService::ActOnChanges(
     const AutofillChangeList& changes) {
  DCHECK(sync_processor_.get());
  SyncChangeList new_changes;
  for (AutofillChangeList::const_iterator change = changes.begin();
       change != changes.end(); ++change) {
    switch (change->type()) {
      case AutofillChange::ADD:
      case AutofillChange::UPDATE: {
        std::vector<base::Time> timestamps;
        if (!web_data_service_->GetDatabase()->
                GetAutofillTable()->GetAutofillTimestamps(
                    change->key().name(),
                    change->key().value(),
                    &timestamps)) {
          NOTREACHED();
          return;
        }
        AutofillEntry entry(change->key(), timestamps);
        SyncChange::SyncChangeType change_type =
           (change->type() == AutofillChange::ADD) ?
               SyncChange::ACTION_ADD : SyncChange::ACTION_UPDATE;
        new_changes.push_back(SyncChange(change_type,
                                         CreateSyncData(entry)));
        break;
      }
      case AutofillChange::REMOVE: {
        std::vector<base::Time> timestamps;
        AutofillEntry entry(change->key(), timestamps);
        new_changes.push_back(SyncChange(SyncChange::ACTION_DELETE,
                                         CreateSyncData(entry)));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet()) {
    DLOG(WARNING) << "[AUTOCOMPLETE SYNC]"
                  << " Failed processing change:"
                  << " Error:" << error.message();
  }
}

SyncData AutocompleteSyncableService::CreateSyncData(
    const AutofillEntry& entry) const {
  sync_pb::EntitySpecifics autofill_specifics;
  WriteAutofillEntry(entry, &autofill_specifics);
  std::string tag(KeyToTag(UTF16ToUTF8(entry.key().name()),
                           UTF16ToUTF8(entry.key().value())));
  return SyncData::CreateLocalData(tag, tag, autofill_specifics);
}

// static
std::string AutocompleteSyncableService::KeyToTag(const std::string& name,
                                                  const std::string& value) {
  std::string ns(kAutofillEntryNamespaceTag);
  return ns + net::EscapePath(name) + "|" + net::EscapePath(value);
}
