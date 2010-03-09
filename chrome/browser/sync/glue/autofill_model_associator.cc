// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_model_associator.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/web_database.h"
#include "net/base/escape.h"

namespace browser_sync {

const char kAutofillTag[] = "google_chrome_autofill";

AutofillModelAssociator::AutofillModelAssociator(
    ProfileSyncService* sync_service,
    WebDatabase* web_database,
    UnrecoverableErrorHandler* error_handler)
    : sync_service_(sync_service),
      web_database_(web_database),
      error_handler_(error_handler),
      autofill_node_id_(sync_api::kInvalidId) {
  DCHECK(sync_service_);
  DCHECK(web_database_);
  DCHECK(error_handler_);
}

bool AutofillModelAssociator::AssociateModels() {
  LOG(INFO) << "Associating Autofill Models";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));

  // TODO(zork): Attempt to load the model association from storage.
  std::vector<AutofillEntry> entries;
  if (!web_database_->GetAllAutofillEntries(&entries)) {
    LOG(ERROR) << "Could not get the autofill entries.";
    return false;
  }

  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode autofill_root(&trans);
  if (!autofill_root.InitByTagLookup(kAutofillTag)) {
    error_handler_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level autofill node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  std::set<AutofillKey> current_entries;
  std::vector<AutofillEntry> new_entries;

  for (std::vector<AutofillEntry>::iterator ix = entries.begin();
       ix != entries.end(); ++ix) {
    if (id_map_.find(ix->key()) != id_map_.end()) {
      // It seems that name/value pairs are not unique in the web database.
      // As a result, we have to filter out duplicates here.  This is probably
      // a bug in the database.
      continue;
    }

    std::string tag = KeyToTag(ix->key().name(), ix->key().value());

    sync_api::ReadNode node(&trans);
    if (node.InitByClientTagLookup(syncable::AUTOFILL, tag)) {
      const sync_pb::AutofillSpecifics& autofill(node.GetAutofillSpecifics());
      DCHECK_EQ(tag, KeyToTag(UTF8ToUTF16(autofill.name()),
                              UTF8ToUTF16(autofill.value())));

      std::vector<base::Time> timestamps;
      if (MergeTimestamps(autofill, ix->timestamps(), &timestamps)) {
        AutofillEntry new_entry(ix->key(), timestamps);
        new_entries.push_back(new_entry);

        sync_api::WriteNode write_node(&trans);
        if (!write_node.InitByClientTagLookup(syncable::AUTOFILL, tag)) {
          LOG(ERROR) << "Failed to write autofill sync node.";
          error_handler_->OnUnrecoverableError();
          return false;
        }
        AutofillChangeProcessor::WriteAutofill(&write_node, new_entry);
      }

      Associate(&(ix->key()), node.GetId());
    } else {
      sync_api::WriteNode node(&trans);
      if (!node.InitUniqueByCreation(syncable::AUTOFILL, autofill_root, tag)) {
        LOG(ERROR) << "Failed to create autofill sync node.";
        error_handler_->OnUnrecoverableError();
        return false;
      }
      node.SetTitle(UTF16ToWide(ix->key().name() + ix->key().value()));
      AutofillChangeProcessor::WriteAutofill(&node, *ix);
      Associate(&(ix->key()), node.GetId());
    }

    current_entries.insert(ix->key());
  }

  int64 sync_child_id = autofill_root.GetFirstChildId();
  while (sync_child_id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_child_node(&trans);
    if (!sync_child_node.InitByIdLookup(sync_child_id)) {
      LOG(ERROR) << "Failed to fetch child node.";
      sync_service_->OnUnrecoverableError();
      return false;
    }
    const sync_pb::AutofillSpecifics& autofill(
      sync_child_node.GetAutofillSpecifics());
    AutofillKey key(UTF8ToUTF16(autofill.name()),
                    UTF8ToUTF16(autofill.value()));

    if (current_entries.find(key) == current_entries.end()) {
      std::vector<base::Time> timestamps;
      int timestamps_count = autofill.usage_timestamp_size();
      for (int c = 0; c < timestamps_count; ++c) {
        timestamps.push_back(base::Time::FromInternalValue(
            autofill.usage_timestamp(c)));
      }
      Associate(&key, sync_child_node.GetId());
      new_entries.push_back(AutofillEntry(key, timestamps));
    }

    sync_child_id = sync_child_node.GetSuccessorId();
  }

  if (new_entries.size() &&
      !web_database_->UpdateAutofillEntries(new_entries)) {
    LOG(ERROR) << "Failed to update autofill entries.";
    error_handler_->OnUnrecoverableError();
    return false;
  }

  return true;
}

bool AutofillModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool AutofillModelAssociator::SyncModelHasUserCreatedNodes() {
  int64 autofill_sync_id;
  if (!GetSyncIdForTaggedNode(kAutofillTag, &autofill_sync_id)) {
    error_handler_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level autofill node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());

  sync_api::ReadNode autofill_node(&trans);
  if (!autofill_node.InitByIdLookup(autofill_sync_id)) {
    error_handler_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level autofill node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the autofill folder has any
  // children.
  return sync_api::kInvalidId != autofill_node.GetFirstChildId();
}

bool AutofillModelAssociator::ChromeModelHasUserCreatedNodes() {
  // Assume the autofill model always have user-created nodes.
  return true;
}

int64 AutofillModelAssociator::GetSyncIdFromChromeId(
    const AutofillKey autofill) {
  AutofillToSyncIdMap::const_iterator iter = id_map_.find(autofill);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void AutofillModelAssociator::Associate(
    const AutofillKey* autofill, int64 sync_id) {
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(*autofill) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[*autofill] = sync_id;
  id_map_inverse_[sync_id] = *autofill;
}

void AutofillModelAssociator::Disassociate(int64 sync_id) {
  SyncIdToAutofillMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  CHECK(id_map_.erase(iter->second));
  id_map_inverse_.erase(iter);
}

bool AutofillModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                     int64* sync_id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

// static
std::string AutofillModelAssociator::KeyToTag(const string16& name,
                                              const string16& value) {
  return EscapePath(UTF16ToUTF8(name)) + "|" + EscapePath(UTF16ToUTF8(value));
}

// static
bool AutofillModelAssociator::MergeTimestamps(
    const sync_pb::AutofillSpecifics& autofill,
    const std::vector<base::Time>& timestamps,
    std::vector<base::Time>* new_timestamps) {
  DCHECK(new_timestamps);
  std::set<base::Time> timestamp_union(timestamps.begin(),
                                       timestamps.end());

  size_t timestamps_count = autofill.usage_timestamp_size();

  bool different = timestamps.size() != timestamps_count;
  for (size_t c = 0; c < timestamps_count; ++c) {
    if (timestamp_union.insert(base::Time::FromInternalValue(
            autofill.usage_timestamp(c))).second) {
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

}  // namespace browser_sync
