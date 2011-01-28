// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_change_processor.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/do_optimistic_refresh_task.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace browser_sync {

AutofillProfileChangeProcessor::AutofillProfileChangeProcessor(
      AutofillProfileModelAssociator *model_associator,
      WebDatabase* web_database,
      PersonalDataManager* personal_data_manager,
      UnrecoverableErrorHandler* error_handler)
      : ChangeProcessor(error_handler),
        model_associator_(model_associator),
        observing_(false),
        web_database_(web_database),
        personal_data_(personal_data_manager) {
  DCHECK(model_associator);
  DCHECK(web_database);
  DCHECK(error_handler);
  DCHECK(personal_data_manager);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  StartObserving();
}

AutofillProfileChangeProcessor::~AutofillProfileChangeProcessor() {}

AutofillProfileChangeProcessor::ScopedStopObserving::ScopedStopObserving(
    AutofillProfileChangeProcessor* processor) {
  processor_ = processor;
  processor_->StopObserving();
}

AutofillProfileChangeProcessor::ScopedStopObserving::~ScopedStopObserving() {
  processor_->StartObserving();
}

void AutofillProfileChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction *write_trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {

  ScopedStopObserving observer(this);

  sync_api::ReadNode autofill_profile_root(write_trans);
  if (!autofill_profile_root.InitByTagLookup(kAutofillProfileTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
      "Autofill Profile root node lookup failed");
    return;
  }

  for (int i = 0; i < change_count; ++i) {
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      DCHECK(changes[i].specifics.HasExtension(
          sync_pb::autofill_profile));

      const sync_pb::AutofillProfileSpecifics& specifics =
          changes[i].specifics.GetExtension(sync_pb::autofill_profile);

      autofill_changes_.push_back(AutofillProfileChangeRecord(changes[i].action,
          changes[i].id,
          specifics));
      continue;
    }

    // If it is not a delete.
    sync_api::ReadNode sync_node(write_trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "Could not find the id in sync db " << changes[i].id;
      continue;
    }

    const sync_pb::AutofillProfileSpecifics& autofill(
        sync_node.GetAutofillProfileSpecifics());

    autofill_changes_.push_back(AutofillProfileChangeRecord(changes[i].action,
        changes[i].id,
        autofill));
  }
}

void AutofillProfileChangeProcessor::Observe(NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::AUTOFILL_PROFILE_CHANGED_GUID);
  WebDataService* wds = Source<WebDataService>(source).ptr();

  if (!wds || wds->GetDatabase() != web_database_)
    return;

  sync_api::WriteTransaction trans(share_handle());
  sync_api::ReadNode autofill_root(&trans);
  if (!autofill_root.InitByTagLookup(kAutofillProfileTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create a tolp level node");
    return;
  }

  AutofillProfileChangeGUID* change =
      Details<AutofillProfileChangeGUID>(details).ptr();

  ActOnChange(change, &trans, autofill_root);
}

void AutofillProfileChangeProcessor::ActOnChange(
     AutofillProfileChangeGUID* change,
     sync_api::WriteTransaction* trans,
     sync_api::ReadNode& autofill_root) {
  DCHECK(change->type() == AutofillProfileChangeGUID::REMOVE ||
         change->profile());
  switch (change->type()) {
    case AutofillProfileChangeGUID::ADD: {
      AddAutofillProfileSyncNode(trans, autofill_root, *(change->profile()));
      break;
    }
    case AutofillProfileChangeGUID::UPDATE: {
      int64 sync_id = model_associator_->GetSyncIdFromChromeId(change->key());
      if (sync_api::kInvalidId == sync_id) {
        LOG(ERROR) << "Sync id is not found for " << change->key();
        break;
      }
      sync_api::WriteNode node(trans);
      if (!node.InitByIdLookup(sync_id)) {
        LOG(ERROR) << "Could not find sync node for id " << sync_id;
        break;
      }

      WriteAutofillProfile(*(change->profile()), &node);
      break;
    }
    case AutofillProfileChangeGUID::REMOVE: {
      int64 sync_id = model_associator_->GetSyncIdFromChromeId(change->key());
      if (sync_api::kInvalidId == sync_id) {
        LOG(ERROR) << "Sync id is not found for " << change->key();
        break;
      }
      sync_api::WriteNode node(trans);
      if (!node.InitByIdLookup(sync_id)) {
        LOG(ERROR) << "Could not find sync node for id " << sync_id;
        break;
      }
      node.Remove();
      model_associator_->Disassociate(sync_id);
      break;
    }
    default:
      NOTREACHED();
  }
}

void AutofillProfileChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!running())
    return;

  ScopedStopObserving observer(this);

  for (unsigned int i = 0;i < autofill_changes_.size(); ++i) {
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        autofill_changes_[i].action_) {
      if (!web_database_->RemoveAutoFillProfile(
          autofill_changes_[i].profile_specifics_.guid())) {
        LOG(ERROR) << "could not delete the profile " <<
           autofill_changes_[i].profile_specifics_.guid();
        continue;
      }
        continue;
    }

    // Now for updates and adds.
    ApplyAutofillProfileChange(autofill_changes_[i].action_,
        autofill_changes_[i].profile_specifics_,
        autofill_changes_[i].id_);
  }

  autofill_changes_.clear();

  PostOptimisticRefreshTask();
}

void AutofillProfileChangeProcessor::PostOptimisticRefreshTask() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      new DoOptimisticRefreshForAutofill(
           personal_data_));
}

void AutofillProfileChangeProcessor::ApplyAutofillProfileChange(
    sync_api::SyncManager::ChangeRecord::Action action,
    const sync_pb::AutofillProfileSpecifics& profile_specifics,
    int64 sync_id) {

  DCHECK_NE(sync_api::SyncManager::ChangeRecord::ACTION_DELETE, action);
  switch (action) {
    case sync_api::SyncManager::ChangeRecord::ACTION_ADD: {
      AutoFillProfile p(profile_specifics.guid());
      AutofillProfileModelAssociator::OverwriteProfileWithServerData(&p,
          profile_specifics);
      if (!web_database_->AddAutoFillProfile(p)) {
        LOG(ERROR) << "could not add autofill profile for guid " << p.guid();
        break;
      }

      // Now that the node has been succesfully created we can associate it.
      std::string guid = p.guid();
      model_associator_->Associate(&guid, sync_id);
      break;
    }
    case sync_api::SyncManager::ChangeRecord::ACTION_UPDATE: {
      AutoFillProfile *p;
      if (!web_database_->GetAutoFillProfileForGUID(
          profile_specifics.guid(), &p)) {
        LOG(ERROR) << "Could not find the autofill profile to update for " <<
          profile_specifics.guid();
        break;
      }
      scoped_ptr<AutoFillProfile> autofill_pointer(p);
      AutofillProfileModelAssociator::OverwriteProfileWithServerData(
          autofill_pointer.get(),
          profile_specifics);

      if (!web_database_->UpdateAutoFillProfile(*(autofill_pointer.get()))) {
        LOG(ERROR) << "Could not update autofill profile for " <<
            profile_specifics.guid();
        break;
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void AutofillProfileChangeProcessor::RemoveSyncNode(const std::string& guid,
    sync_api::WriteTransaction* trans) {
  sync_api::WriteNode node(trans);
  int64 sync_id = model_associator_->GetSyncIdFromChromeId(guid);
  if (sync_api::kInvalidId == sync_id) {
    LOG(ERROR) << "Could not find the node in associator " << guid;
    return;
  }

  if (!node.InitByIdLookup(sync_id)) {
    LOG(ERROR) << "Could not find the sync node for " << guid;
    return;
  }

  model_associator_->Disassociate(sync_id);
  node.Remove();
}

void AutofillProfileChangeProcessor::AddAutofillProfileSyncNode(
    sync_api::WriteTransaction* trans,
    sync_api::BaseNode& autofill_profile_root,
    const AutoFillProfile& profile) {
  sync_api::WriteNode node(trans);
  if (!node.InitUniqueByCreation(syncable::AUTOFILL_PROFILE,
      autofill_profile_root,
      profile.guid())) {
    LOG(ERROR) << "could not create a sync node ";
    return;
  }

  node.SetTitle(UTF8ToWide(profile.guid()));

  WriteAutofillProfile(profile, &node);

  std::string guid = profile.guid();
  model_associator_->Associate(&guid, node.GetId());
}

void AutofillProfileChangeProcessor::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.Add(this,
      NotificationType::AUTOFILL_PROFILE_CHANGED_GUID,
      NotificationService::AllSources());
}

void AutofillProfileChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  notification_registrar_.RemoveAll();
}

void AutofillProfileChangeProcessor::WriteAutofillProfile(
    const AutoFillProfile& profile,
    sync_api::WriteNode* node) {
  sync_pb::AutofillProfileSpecifics specifics;
  specifics.set_guid(profile.guid());
  specifics.set_name_first(UTF16ToUTF8(
      profile.GetFieldText(AutoFillType(NAME_FIRST))));
  specifics.set_name_middle(UTF16ToUTF8(
      profile.GetFieldText(AutoFillType(NAME_MIDDLE))));
  specifics.set_name_last(
      UTF16ToUTF8(profile.GetFieldText(AutoFillType(NAME_LAST))));
  specifics.set_address_home_line1(
      UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1))));
  specifics.set_address_home_line2(
      UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2))));
  specifics.set_address_home_city(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_CITY))));
  specifics.set_address_home_state(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_STATE))));
  specifics.set_address_home_country(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_COUNTRY))));
  specifics.set_address_home_zip(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(ADDRESS_HOME_ZIP))));
  specifics.set_email_address(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(EMAIL_ADDRESS))));
  specifics.set_company_name(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(COMPANY_NAME))));
  specifics.set_phone_fax_whole_number(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(PHONE_FAX_WHOLE_NUMBER))));
  specifics.set_phone_home_whole_number(UTF16ToUTF8(profile.GetFieldText(
      AutoFillType(PHONE_HOME_WHOLE_NUMBER))));
  node->SetAutofillProfileSpecifics(specifics);
}

}  // namespace browser_sync

