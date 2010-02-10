// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_change_processor.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

PreferenceChangeProcessor::PreferenceChangeProcessor(
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      pref_service_(NULL),
      model_associator_(NULL) {
}

void PreferenceChangeProcessor::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(running());
  DCHECK(NotificationType::PREF_CHANGED == type);
  DCHECK_EQ(pref_service_, Source<PrefService>(source).ptr());

  std::wstring* name = Details<std::wstring>(details).ptr();
  const PrefService::Preference* preference =
      pref_service_->FindPreference((*name).c_str());
  DCHECK(preference);

  sync_api::WriteTransaction trans(share_handle());
  sync_api::WriteNode sync_node(&trans);

  int64 sync_id = model_associator_->GetSyncIdFromChromeId(*name);
  if (sync_api::kInvalidId == sync_id) {
    LOG(ERROR) << "Unexpected notification for: " << *name;
    error_handler()->OnUnrecoverableError();
    return;
  } else {
    if (!sync_node.InitByIdLookup(sync_id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }
  }

  if (!WritePreference(&sync_node, WideToUTF8(*name),
                       preference->GetValue())) {
    LOG(ERROR) << "Failed to update preference node.";
    error_handler()->OnUnrecoverableError();
    return;
  }
}

void PreferenceChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!running())
    return;
  StopObserving();

  // TODO(sync): Remove this once per-datatype filtering is working.
  sync_api::ReadNode root_node(trans);
  if (!root_node.InitByTagLookup(kPreferencesTag)) {
    LOG(ERROR) << "Preference root node lookup failed.";
    error_handler()->OnUnrecoverableError();
    return;
  }

  sync_api::ReadNode sync_node(trans);
  std::string name;
  for (int i = 0; i < change_count; ++i) {
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }

    // TODO(sync): Remove this once per-datatype filtering is working.
    // Check that the changed node is a child of the preferences folder.
    if (root_node.GetId() != sync_node.GetParentId()) continue;

    scoped_ptr<Value> value(ReadPreference(&sync_node, &name));
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      pref_service_->ClearPref(UTF8ToWide(name).c_str());
    } else {
      if (value.get() && pref_service_->HasPrefPath(UTF8ToWide(name).c_str())) {
        pref_service_->Set(UTF8ToWide(name).c_str(), *value);
      }
    }
  }
  StartObserving();
}

bool PreferenceChangeProcessor::WritePreference(
    sync_api::WriteNode* node,
    const std::string& name,
    const Value* value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(*value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    error_handler()->OnUnrecoverableError();
    return false;
  }

  sync_pb::PreferenceSpecifics preference;
  preference.set_name(name);
  preference.set_value(serialized);
  node->SetPreferenceSpecifics(preference);
  return true;
}

Value* PreferenceChangeProcessor::ReadPreference(
    sync_api::ReadNode* node,
    std::string* name) {
  const sync_pb::PreferenceSpecifics& preference(
      node->GetPreferenceSpecifics());
  JSONStringValueSerializer json(preference.value());

  std::string error_message;
  Value* value = json.Deserialize(&error_message);
  if (!value) {
    LOG(ERROR) << "Failed to deserialize preference: " << error_message;
    error_handler()->OnUnrecoverableError();
    return NULL;
  }
  *name = preference.name();
  return value;
}

void PreferenceChangeProcessor::StartImpl(Profile* profile) {
  pref_service_ = profile->GetPrefs();
  StartObserving();
}

void PreferenceChangeProcessor::StopImpl() {
  StopObserving();
  pref_service_ = NULL;
}


void PreferenceChangeProcessor::StartObserving() {
  DCHECK(model_associator_ && pref_service_);
  for (std::set<std::wstring>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    pref_service_->AddPrefObserver((*it).c_str(), this);
  }
}

void PreferenceChangeProcessor::StopObserving() {
  DCHECK(model_associator_ && pref_service_);
  for (std::set<std::wstring>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    pref_service_->RemovePrefObserver((*it).c_str(), this);
  }
}

}  // namespace browser_sync
