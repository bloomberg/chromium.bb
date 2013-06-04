// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/managed_mode/managed_user_refresh_token_fetcher.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using base::DictionaryValue;
using syncer::MANAGED_USERS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;
using sync_pb::ManagedUserSpecifics;
using user_prefs::PrefRegistrySyncable;

namespace {

const char kAcknowledged[] = "acknowledged";
const char kName[] = "name";

SyncData CreateLocalSyncData(const std::string& id,
                             const std::string& name,
                             bool acknowledged) {
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  if (acknowledged)
    specifics.mutable_managed_user()->set_acknowledged(true);
  return SyncData::CreateLocalData(id, name, specifics);
}

}  // namespace

ManagedUserRegistrationService::ManagedUserRegistrationService(
    PrefService* prefs,
    scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher)
    : weak_ptr_factory_(this),
      prefs_(prefs),
      token_fetcher_(token_fetcher.Pass()),
      pending_managed_user_acknowledged_(false) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kGoogleServicesLastUsername,
      base::Bind(&ManagedUserRegistrationService::OnLastSignedInUsernameChange,
                 base::Unretained(this)));
}

ManagedUserRegistrationService::~ManagedUserRegistrationService() {
  DCHECK(pending_managed_user_id_.empty());
  DCHECK(callback_.is_null());
}

// static
void ManagedUserRegistrationService::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kManagedUsers,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void ManagedUserRegistrationService::Register(
    const string16& name,
    const RegistrationCallback& callback) {
  DCHECK(pending_managed_user_id_.empty());
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  DictionaryValue* dict = update.Get();
  DictionaryValue* value = new DictionaryValue;
  value->SetString(kName, name);
  std::string id_raw = base::RandBytesAsString(8);
  bool success = base::Base64Encode(id_raw, &pending_managed_user_id_);
  DCHECK(success);
  DCHECK(!dict->HasKey(pending_managed_user_id_));
  dict->SetWithoutPathExpansion(pending_managed_user_id_, value);

  if (sync_processor_) {
    // If we're already syncing, create a new change and upload it.
    SyncChangeList change_list;
    change_list.push_back(SyncChange(
        FROM_HERE,
        SyncChange::ACTION_ADD,
        CreateLocalSyncData(
            pending_managed_user_id_, base::UTF16ToUTF8(name), false)));
    SyncError error =
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    DCHECK(!error.IsSet()) << error.ToString();
  }

  callback_ = callback;
  browser_sync::DeviceInfo::CreateLocalDeviceInfo(
      base::Bind(&ManagedUserRegistrationService::FetchToken,
                 weak_ptr_factory_.GetWeakPtr(), name));
}

void ManagedUserRegistrationService::Shutdown() {
  CancelPendingRegistration();
}

SyncMergeResult ManagedUserRegistrationService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(MANAGED_USERS, type);
  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  SyncChangeList change_list;
  SyncMergeResult result(MANAGED_USERS);

  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  DictionaryValue* dict = update.Get();
  result.set_num_items_before_association(dict->size());
  std::set<std::string> seen_ids;
  int num_items_added = 0;
  int num_items_modified = 0;
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    DCHECK_EQ(MANAGED_USERS, it->GetDataType());
    const ManagedUserSpecifics& managed_user =
        it->GetSpecifics().managed_user();
    DictionaryValue* value = new DictionaryValue();
    value->SetString(kName, managed_user.name());
    DCHECK(managed_user.acknowledged());
    value->SetBoolean(kAcknowledged, managed_user.acknowledged());
    if (dict->HasKey(managed_user.id()))
      num_items_modified++;
    else
      num_items_added++;
    dict->SetWithoutPathExpansion(managed_user.id(), value);
    seen_ids.insert(managed_user.id());
  }

  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (seen_ids.find(it.key()) != seen_ids.end())
      continue;

    const DictionaryValue* dict = NULL;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);
    bool acknowledged = false;
    dict->GetBoolean(kAcknowledged, &acknowledged);
    std::string name;
    dict->GetString(kName, &name);
    change_list.push_back(
        SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                   CreateLocalSyncData(it.key(), name, acknowledged)));
  }
  result.set_error(sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));

  result.set_num_items_modified(num_items_modified);
  result.set_num_items_added(num_items_added);
  result.set_num_items_after_association(dict->size());

  return result;
}

void ManagedUserRegistrationService::StopSyncing(ModelType type) {
  DCHECK_EQ(MANAGED_USERS, type);
  sync_processor_.reset();
  error_handler_.reset();

  CancelPendingRegistration();
}

SyncDataList ManagedUserRegistrationService::GetAllSyncData(
    ModelType type) const {
  SyncDataList data;
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  DictionaryValue* dict = update.Get();
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const DictionaryValue* dict = NULL;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);
    std::string name;
    dict->GetString(kName, &name);
    bool acknowledged = false;
    dict->GetBoolean(kAcknowledged, &acknowledged);
    data.push_back(CreateLocalSyncData(it.key(), name, acknowledged));
  }
  return data;
}

SyncError ManagedUserRegistrationService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  SyncError error;
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  DictionaryValue* dict = update.Get();
  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    SyncData data = it->sync_data();
    DCHECK_EQ(MANAGED_USERS, data.GetDataType());
    const ManagedUserSpecifics& managed_user =
        data.GetSpecifics().managed_user();
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        // Every item we get from the server should be acknowledged.
        DCHECK(managed_user.acknowledged());
        const DictionaryValue* old_value = NULL;
        dict->GetDictionaryWithoutPathExpansion(managed_user.id(), &old_value);

        // For an update action, the managed user should already exist, for an
        // add action, it should not.
        DCHECK_EQ(
            old_value ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD,
            it->change_type());

        // If the managed user switched from unacknowledged to acknowledged,
        // we might need to continue with a registration.
        if (!old_value->HasKey(kAcknowledged))
          OnManagedUserAcknowledged(managed_user.id());

        DictionaryValue* value = new DictionaryValue;
        value->SetString(kName, managed_user.name());
        value->SetBoolean(kAcknowledged, managed_user.acknowledged());
        dict->SetWithoutPathExpansion(managed_user.id(), value);
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DCHECK(dict->HasKey(managed_user.id())) << managed_user.id();
        dict->RemoveWithoutPathExpansion(managed_user.id(), NULL);
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }
  return error;
}

void ManagedUserRegistrationService::OnLastSignedInUsernameChange() {
  DCHECK(!sync_processor_);

  // If the last signed in user changes, we clear all data, to avoid managed
  // users from one custodian appearing in another one's profile.
  prefs_->ClearPref(prefs::kManagedUsers);
}

void ManagedUserRegistrationService::OnManagedUserAcknowledged(
    const std::string& managed_user_id) {
  DCHECK_EQ(pending_managed_user_id_, managed_user_id);
  DCHECK(!pending_managed_user_acknowledged_);
  pending_managed_user_acknowledged_ = true;
  DispatchCallbackIfReady();
}

void ManagedUserRegistrationService::FetchToken(
    const string16& name,
    const browser_sync::DeviceInfo& device_info) {
  token_fetcher_->Start(
      pending_managed_user_id_, name, device_info.client_name(),
      base::Bind(&ManagedUserRegistrationService::OnReceivedToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserRegistrationService::OnReceivedToken(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    DispatchCallback(error);
    return;
  }

  DCHECK(!token.empty());
  pending_managed_user_token_ = token;
  DispatchCallbackIfReady();
}

void ManagedUserRegistrationService::DispatchCallbackIfReady() {
  if (!pending_managed_user_acknowledged_ ||
      pending_managed_user_token_.empty()) {
    return;
  }

  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  DispatchCallback(error);
}

void ManagedUserRegistrationService::CancelPendingRegistration() {
  if (!pending_managed_user_id_.empty()) {
    // Remove a pending managed user if there is one.
    DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
    bool success = update->RemoveWithoutPathExpansion(pending_managed_user_id_,
                                                      NULL);
    DCHECK(success);
  }
  pending_managed_user_token_.clear();
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  DispatchCallback(error);
}

void ManagedUserRegistrationService::DispatchCallback(
    const GoogleServiceAuthError& error) {
  if (callback_.is_null())
    return;

  callback_.Run(error, pending_managed_user_token_);
  callback_.Reset();
  pending_managed_user_token_.clear();
  pending_managed_user_id_.clear();
  pending_managed_user_acknowledged_ = false;
}
