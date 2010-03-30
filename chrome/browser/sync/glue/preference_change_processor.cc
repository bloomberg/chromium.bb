// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_change_processor.h"

#include <set>
#include <string>

#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

PreferenceChangeProcessor::PreferenceChangeProcessor(
    PreferenceModelAssociator* model_associator,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      pref_service_(NULL),
      model_associator_(model_associator) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(model_associator);
  DCHECK(error_handler);
}

PreferenceChangeProcessor::~PreferenceChangeProcessor(){
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void PreferenceChangeProcessor::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(running());
  DCHECK(NotificationType::PREF_CHANGED == type);
  DCHECK_EQ(pref_service_, Source<PrefService>(source).ptr());

  std::wstring* name = Details<std::wstring>(details).ptr();
  const PrefService::Preference* preference =
      pref_service_->FindPreference((*name).c_str());
  DCHECK(preference);

  sync_api::WriteTransaction trans(share_handle());
  sync_api::WriteNode node(&trans);

  int64 sync_id = model_associator_->GetSyncIdFromChromeId(*name);
  if (sync_api::kInvalidId == sync_id) {
    LOG(ERROR) << "Unexpected notification for: " << *name;
    error_handler()->OnUnrecoverableError();
    return;
  } else {
    if (!node.InitByIdLookup(sync_id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }
  }

  if (!WritePreference(&node, *name, preference->GetValue())) {
    LOG(ERROR) << "Failed to update preference node.";
    error_handler()->OnUnrecoverableError();
    return;
  }
}

void PreferenceChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!running())
    return;
  StopObserving();

  for (int i = 0; i < change_count; ++i) {
    sync_api::ReadNode node(trans);
    // TODO(ncarter): Can't look up the name for deletions: lookup of
    // deleted items fails at the syncapi layer.  However, the node should
    // generally still exist in the syncable database; we just need to
    // plumb the syncapi so that it succeeds.
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      // Until the above is fixed, we have no choice but to ignore deletions.
      LOG(ERROR) << "No way to handle pref deletion";
      continue;
    }

    if (!node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }
    DCHECK(syncable::PREFERENCES == node.GetModelType());

    std::string name;
    scoped_ptr<Value> value(ReadPreference(&node, &name));
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      pref_service_->ClearPref(UTF8ToWide(name).c_str());
    } else {
      const PrefService::Preference* preference =
          pref_service_->FindPreference(UTF8ToWide(name).c_str());
      if (value.get() && preference) {
        pref_service_->Set(UTF8ToWide(name).c_str(), *value);
      }
    }
  }
  StartObserving();
}

bool PreferenceChangeProcessor::WritePreference(
    sync_api::WriteNode* node,
    const std::wstring& name,
    const Value* value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(*value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    error_handler()->OnUnrecoverableError();
    return false;
  }

  sync_pb::PreferenceSpecifics preference;
  preference.set_name(WideToUTF8(name));
  preference.set_value(serialized);
  node->SetPreferenceSpecifics(preference);
  node->SetTitle(name);
  return true;
}

Value* PreferenceChangeProcessor::ReadPreference(
    sync_api::ReadNode* node,
    std::string* name) {
  const sync_pb::PreferenceSpecifics& preference(
      node->GetPreferenceSpecifics());
  base::JSONReader reader;
  scoped_ptr<Value> value(reader.JsonToValue(preference.value(), false, false));
  if (!value.get()) {
    LOG(ERROR) << "Failed to deserialize preference value: "
               << reader.error_message();
    error_handler()->OnUnrecoverableError();
    return NULL;
  }
  *name = preference.name();
  return value.release();
}

void PreferenceChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  pref_service_ = profile->GetPrefs();
  StartObserving();
}

void PreferenceChangeProcessor::StopImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  StopObserving();
  pref_service_ = NULL;
}


void PreferenceChangeProcessor::StartObserving() {
  DCHECK(pref_service_);
  for (std::set<std::wstring>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    pref_service_->AddPrefObserver((*it).c_str(), this);
  }
}

void PreferenceChangeProcessor::StopObserving() {
  DCHECK(pref_service_);
  for (std::set<std::wstring>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    pref_service_->RemovePrefObserver((*it).c_str(), this);
  }
}

}  // namespace browser_sync
