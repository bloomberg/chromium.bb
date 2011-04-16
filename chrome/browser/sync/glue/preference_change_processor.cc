// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_change_processor.h"

#include <set>
#include <string>

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/json_value_serializer.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace browser_sync {

PreferenceChangeProcessor::PreferenceChangeProcessor(
    PreferenceModelAssociator* model_associator,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      pref_service_(NULL),
      model_associator_(model_associator),
      processing_pref_change_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(model_associator);
  DCHECK(error_handler);
}

PreferenceChangeProcessor::~PreferenceChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void PreferenceChangeProcessor::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(NotificationType::PREF_CHANGED == type);
  DCHECK_EQ(pref_service_, Source<PrefService>(source).ptr());

  // Avoid recursion.
  if (processing_pref_change_)
    return;

  AutoReset<bool> guard(&processing_pref_change_, true);
  std::string* name = Details<std::string>(details).ptr();
  const PrefService::Preference* preference =
      pref_service_->FindPreference((*name).c_str());
  DCHECK(preference);
  int64 sync_id = model_associator_->GetSyncIdFromChromeId(*name);
  bool user_modifiable = preference->IsUserModifiable();
  if (!user_modifiable) {
    // We do not track preferences the user cannot change.
    model_associator_->Disassociate(sync_id);
    return;
  }

  sync_api::WriteTransaction trans(share_handle());
  sync_api::WriteNode node(&trans);

  // Since we don't create sync nodes for preferences that are not under control
  // of the user or still have their default value, this changed preference may
  // not have a sync node yet. If so, we create a node. Similarly, a preference
  // may become user-modifiable (e.g. due to laxer policy configuration), in
  // which case we also need to create a sync node and associate it.
  if (sync_api::kInvalidId == sync_id) {
    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(browser_sync::kPreferencesTag)) {
      error_handler()->OnUnrecoverableError(FROM_HERE, "Can't find root.");
      return;
    }

    // InitPrefNodeAndAssociate takes care of writing the value to the sync
    // node if appropriate.
    if (!model_associator_->InitPrefNodeAndAssociate(&trans,
                                                     root,
                                                     preference)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
                                            "Can't create sync node.");
    }
    return;
  }

  if (!node.InitByIdLookup(sync_id)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
                                          "Preference node lookup failed.");
    return;
  }

  if (!PreferenceModelAssociator::WritePreferenceToNode(
          preference->name(),
          *preference->GetValue(),
          &node)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
                                          "Failed to update preference node.");
    return;
  }
}

void PreferenceChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
      error_handler()->OnUnrecoverableError(FROM_HERE,
                                            "Preference node lookup failed.");
      return;
    }
    DCHECK(syncable::PREFERENCES == node.GetModelType());

    std::string name;
    scoped_ptr<Value> value(ReadPreference(&node, &name));
    // Skip values we can't deserialize.
    if (!value.get())
      continue;

    // It is possible that we may receive a change to a preference we
    // do not want to sync.  For example if the user is syncing a Mac
    // client and a Windows client, the Windows client does not
    // support kConfirmToQuitEnabled.  Ignore updates from these
    // preferences.
    const char* pref_name = name.c_str();
    if (model_associator_->synced_preferences().count(pref_name) == 0)
      continue;

    // Don't try to overwrite preferences not controllable by the user.
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    DCHECK(pref);
    if (!pref->IsUserModifiable())
      continue;

    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      pref_service_->ClearPref(pref_name);
    } else {
      pref_service_->Set(pref_name, *value);

      // If this is a newly added node, associate.
      if (sync_api::SyncManager::ChangeRecord::ACTION_ADD ==
          changes[i].action) {
        const PrefService::Preference* preference =
            pref_service_->FindPreference(name.c_str());
        model_associator_->Associate(preference, changes[i].id);
      }

      model_associator_->AfterUpdateOperations(name);
    }
  }
  StartObserving();
}

Value* PreferenceChangeProcessor::ReadPreference(
    sync_api::ReadNode* node,
    std::string* name) {
  const sync_pb::PreferenceSpecifics& preference(
      node->GetPreferenceSpecifics());
  base::JSONReader reader;
  scoped_ptr<Value> value(reader.JsonToValue(preference.value(), false, false));
  if (!value.get()) {
    std::string err = "Failed to deserialize preference value: " +
        reader.GetErrorMessage();
    error_handler()->OnUnrecoverableError(FROM_HERE, err);
    return NULL;
  }
  *name = preference.name();
  return value.release();
}

void PreferenceChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_service_ = profile->GetPrefs();
  registrar_.Init(pref_service_);
  StartObserving();
}

void PreferenceChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StopObserving();
  pref_service_ = NULL;
}


void PreferenceChangeProcessor::StartObserving() {
  DCHECK(pref_service_);
  for (std::set<std::string>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    registrar_.Add((*it).c_str(), this);
  }
}

void PreferenceChangeProcessor::StopObserving() {
  DCHECK(pref_service_);
  for (std::set<std::string>::const_iterator it =
      model_associator_->synced_preferences().begin();
      it != model_associator_->synced_preferences().end(); ++it) {
    registrar_.Remove((*it).c_str(), this);
  }
}

}  // namespace browser_sync
